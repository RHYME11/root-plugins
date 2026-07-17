#include <PhotoPeakFit/PhotoPeakFitter.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include <TCanvas.h>
#include <TF1.h>
#include <TFitResult.h>
#include <TFitResultPtr.h>
#include <TH1.h>
#include <TList.h>
#include <TMath.h>
#include <TMatrixDSym.h>
#include <TObject.h>
#include <TString.h>
#include <TVirtualPad.h>

namespace {

struct Candidate {
  const char* name;
  bool useTail;
  bool useStep;
  bool useQuadBg;
};

struct Trial {
  TF1* function = nullptr;
  TMatrixDSym covariance{kPhotoPeakNPars};
  Candidate candidate{};
  int status = 1;
  int ndf = 0;
  int freeParameters = 0;
  double chi2 = 0.0;
  double reducedChi2 = 0.0;
};

class PhotoPeakEvaluator {
  public:
    explicit PhotoPeakEvaluator(double midpoint) : fMidpoint(midpoint) {
    }

    // ============== PhotoPeakEvaluator::operator() ==============
    // Purpose: Evaluate the RadWare/GF3-style single-photopeak function.
    // Inputs: ROOT x and parameter arrays.
    // Outputs: Total peak, background, and smoothed-step value.
    double operator()(double* x, double* par) const {
      const double xx = x[0];
      const double beta = std::max(par[kPhotoPeakBeta], 1.0e-12);
      const double fwhm = std::max(par[kPhotoPeakFwhm], 1.0e-12);
      const double sigma = fwhm / 2.35482;
      const double w = (xx - par[kPhotoPeakPosition]) /
        (sigma * TMath::Sqrt2());
      const double y = fwhm / (beta * 3.33021838);
      const double erfcY = std::max(TMath::Erfc(y), 1.0e-300);
      const double gaussian = std::exp(-w * w);
      const double stepShape = TMath::Erfc(w);
      double skew = 0.0;

      const double tailArgument = (xx - par[kPhotoPeakPosition]) / beta;
      if(std::fabs(tailArgument) <= 700.0) {
        skew = std::exp(tailArgument) * TMath::Erfc(w + y) / erfcY;
      }

      const double tailFraction = par[kPhotoPeakR] / 100.0;
      const double centered = xx - fMidpoint;
      const double background = par[kPhotoPeakA] +
        par[kPhotoPeakB] * centered +
        par[kPhotoPeakC] * centered * centered;
      const double step = par[kPhotoPeakHeight] * par[kPhotoPeakStep] *
        stepShape / 200.0;
      const double peak = par[kPhotoPeakHeight] *
        ((1.0 - tailFraction) * gaussian + tailFraction * skew);
      return peak + background + step;
    }

  private:
    double fMidpoint;
};

class PhotoPeakBackgroundEvaluator {
  public:
    explicit PhotoPeakBackgroundEvaluator(double midpoint)
      : fMidpoint(midpoint) {
    }

    // ============== PhotoPeakBackgroundEvaluator::operator() ==============
    // Purpose: Evaluate quadratic plus smoothed-step background.
    // Inputs: ROOT x and fitted parameter arrays.
    // Outputs: Background-only function value.
    double operator()(double* x, double* par) const {
      const double fwhm = std::max(par[kPhotoPeakFwhm], 1.0e-12);
      const double sigma = fwhm / 2.35482;
      const double w = (x[0] - par[kPhotoPeakPosition]) /
        (sigma * TMath::Sqrt2());
      const double centered = x[0] - fMidpoint;
      const double background = par[kPhotoPeakA] +
        par[kPhotoPeakB] * centered +
        par[kPhotoPeakC] * centered * centered;
      const double step = par[kPhotoPeakHeight] * par[kPhotoPeakStep] *
        TMath::Erfc(w) / 200.0;
      return background + step;
    }

  private:
    double fMidpoint;
};

// ============== BinContentAt ==============
// Purpose: Read the histogram bin content at one x-value.
// Inputs: Histogram and x-value.
// Outputs: Clamped bin content.
double BinContentAt(TH1* hist, double x) {
  const int bin = hist->GetXaxis()->FindFixBin(x);
  const int clamped = std::max(1, std::min(hist->GetNbinsX(), bin));
  return hist->GetBinContent(clamped);
}

// ============== MaximumInRange ==============
// Purpose: Find the maximum histogram content in an x range.
// Inputs: Histogram and ordered fit limits.
// Outputs: Maximum bin content.
double MaximumInRange(TH1* hist, double low, double high) {
  int lowBin = hist->GetXaxis()->FindFixBin(low);
  int highBin = hist->GetXaxis()->FindFixBin(high);
  lowBin = std::max(1, std::min(hist->GetNbinsX(), lowBin));
  highBin = std::max(1, std::min(hist->GetNbinsX(), highBin));
  if(lowBin > highBin)
    std::swap(lowBin, highBin);

  double maximum = hist->GetBinContent(lowBin);
  for(int bin = lowBin + 1; bin <= highBin; ++bin)
    maximum = std::max(maximum, hist->GetBinContent(bin));
  return maximum;
}

// ============== BinCountInRange ==============
// Purpose: Count histogram bins touched by an x range.
// Inputs: Histogram and ordered fit limits.
// Outputs: Inclusive bin count.
int BinCountInRange(TH1* hist, double low, double high) {
  int lowBin = hist->GetXaxis()->FindFixBin(low);
  int highBin = hist->GetXaxis()->FindFixBin(high);
  lowBin = std::max(1, std::min(hist->GetNbinsX(), lowBin));
  highBin = std::max(1, std::min(hist->GetNbinsX(), highBin));
  if(lowBin > highBin)
    std::swap(lowBin, highBin);
  return highBin - lowBin + 1;
}

// ============== FreeParameterCount ==============
// Purpose: Count active parameters in one candidate model.
// Inputs: Candidate model flags.
// Outputs: Number of free parameters before fitting.
int FreeParameterCount(const Candidate& candidate) {
  int count = 5;
  if(candidate.useTail)
    count += 2;
  if(candidate.useStep)
    ++count;
  if(candidate.useQuadBg)
    ++count;
  return count;
}

// ============== ConfigureFunction ==============
// Purpose: Apply original PhotoPeakFit defaults and model constraints.
// Inputs: Function, candidate, defaults, fit limits, and safety limits.
// Outputs: Configured ROOT fit function.
void ConfigureFunction(TF1* function, const Candidate& candidate,
                       double a0, double b0, double c0,
                       double r0, double beta0, double step0,
                       double peak0, double fwhm0, double height0,
                       double fitLow, double fitHigh,
                       double range, double heightUpper) {
  function->SetParNames("A", "B", "C", "R", "BETA", "STEP", "P", "W", "H");
  function->SetParameters(a0, b0, c0, r0, beta0, step0,
                          peak0, fwhm0, height0);
  function->SetParLimits(kPhotoPeakR, 0.0, 100.0);
  function->SetParLimits(kPhotoPeakBeta, 1.0e-6, 10.0 * range);
  function->SetParLimits(kPhotoPeakStep, 0.0, 100.0);
  function->SetParLimits(kPhotoPeakPosition, fitLow, fitHigh);
  function->SetParLimits(kPhotoPeakFwhm, 1.0e-6, range);
  function->SetParLimits(kPhotoPeakHeight, 0.0, heightUpper);
  if(!candidate.useTail) {
    function->FixParameter(kPhotoPeakR, 0.0);
    function->FixParameter(kPhotoPeakBeta, beta0);
  }
  if(!candidate.useStep)
    function->FixParameter(kPhotoPeakStep, 0.0);
  if(!candidate.useQuadBg)
    function->FixParameter(kPhotoPeakC, 0.0);
  function->SetNpx(2000);
}

// ============== TrialIsBetter ==============
// Purpose: Compare two auto-mode fit trials.
// Inputs: New trial and current best trial.
// Outputs: True when the new trial should replace the best trial.
bool TrialIsBetter(const Trial* trial, const Trial* best) {
  if(!trial || trial->ndf <= 0 || !std::isfinite(trial->reducedChi2))
    return false;
  if(!best || best->ndf <= 0 || !std::isfinite(best->reducedChi2))
    return true;
  if(trial->status == 0 && best->status != 0)
    return true;
  if(trial->status != 0 && best->status == 0)
    return false;
  return trial->reducedChi2 < best->reducedChi2;
}

// ============== PeakArea ==============
// Purpose: Calculate the GF3 peak-only area in histogram-bin counts.
// Inputs: Fit parameters and local bin width.
// Outputs: Integrated photopeak area.
double PeakArea(const double* parameters, double binWidth) {
  const double tailFraction = parameters[kPhotoPeakR] / 100.0;
  const double beta = std::max(parameters[kPhotoPeakBeta], 1.0e-12);
  const double fwhm = std::max(parameters[kPhotoPeakFwhm], 1.0e-12);
  const double height = parameters[kPhotoPeakHeight];
  const double y = fwhm / (beta * 3.33021838);
  const double erfcY = TMath::Erfc(y);
  const double d = erfcY > 0.0 ? std::exp(-y * y) / erfcY : 0.0;
  const double continuousArea = height *
    (tailFraction * 2.0 * beta * d +
     (1.0 - tailFraction) * fwhm * 1.06446705);
  return continuousArea / std::max(binWidth, 1.0e-12);
}

// ============== PeakAreaUncertainty ==============
// Purpose: Propagate covariance into the GF3 photopeak area uncertainty.
// Inputs: Fit parameters, covariance matrix, and local bin width.
// Outputs: One-sigma area uncertainty.
double PeakAreaUncertainty(const double* parameters,
                           const TMatrixDSym& covariance,
                           double binWidth) {
  const double inverseBinWidth = 1.0 / std::max(binWidth, 1.0e-12);
  const double tailFraction = parameters[kPhotoPeakR] / 100.0;
  const double beta = std::max(parameters[kPhotoPeakBeta], 1.0e-12);
  const double fwhm = std::max(parameters[kPhotoPeakFwhm], 1.0e-12);
  const double height = parameters[kPhotoPeakHeight];
  const double y = fwhm / (beta * 3.33021838);
  const double erfcY = TMath::Erfc(y);
  const double d = erfcY > 0.0 ? std::exp(-y * y) / erfcY : 0.0;
  const double dAdH = tailFraction * 2.0 * beta * d +
    (1.0 - tailFraction) * fwhm * 1.06446705;
  const double dAdR = 0.01 * height *
    (2.0 * beta * d - fwhm * 1.06446705);
  const double dAdBeta = height * tailFraction * 2.0 * d *
    (1.0 + 2.0 * y * y - d * 1.12837917 * y);
  const double dAdW = height *
    (tailFraction * 2.0 * 0.600561216 * d *
       (d / 1.77245385 - y) +
     (1.0 - tailFraction) * 1.06446705);

  double gradient[kPhotoPeakNPars] = {0.0};
  gradient[kPhotoPeakR] = dAdR * inverseBinWidth;
  gradient[kPhotoPeakBeta] = dAdBeta * inverseBinWidth;
  gradient[kPhotoPeakFwhm] = dAdW * inverseBinWidth;
  gradient[kPhotoPeakHeight] = dAdH * inverseBinWidth;

  double variance = 0.0;
  for(int row = 0; row < kPhotoPeakNPars; ++row) {
    for(int column = 0; column < kPhotoPeakNPars; ++column) {
      variance += gradient[row] * covariance(row, column) * gradient[column];
    }
  }
  return variance > 0.0 ? std::sqrt(variance) : 0.0;
}

// ============== RemovePreviousDrawObjects ==============
// Purpose: Remove previous minimal PhotoPeakFit curves for one histogram.
// Inputs: Target pad and histogram.
// Outputs: Pad without prior plugin-owned fit functions.
void RemovePreviousDrawObjects(TVirtualPad* pad, TH1* hist) {
  if(!pad || !pad->GetListOfPrimitives())
    return;
  const TString prefix = TString::Format("PhotoPeak_%p_", static_cast<void*>(hist));
  std::vector<TObject*> remove;
  TIter next(pad->GetListOfPrimitives());
  while(TObject* object = next()) {
    if(TString(object->GetName()).BeginsWith(prefix))
      remove.push_back(object);
  }
  for(TObject* object : remove) {
    pad->GetListOfPrimitives()->Remove(object);
    delete object;
  }
}

// ============== PrintResult ==============
// Purpose: Print the minimal fit result in PhotoPeakFit.C format.
// Inputs: Histogram, fit inputs, and result.
// Outputs: Flushed terminal report.
void PrintResult(TH1* hist, double fitLow, double fitHigh, double peak0,
                 const PhotoPeakFitResult& result) {
  const int order[kPhotoPeakNPars] = {
    kPhotoPeakPosition, kPhotoPeakHeight, kPhotoPeakFwhm,
    kPhotoPeakR, kPhotoPeakBeta, kPhotoPeakStep,
    kPhotoPeakA, kPhotoPeakB, kPhotoPeakC
  };
  const char* names[kPhotoPeakNPars] = {
    "A", "B", "C", "R", "BETA", "STEP", "P", "W", "H"
  };

  std::printf("\nphotopeakfit result for %s\n", hist->GetName());
  std::printf("Fit range: [%g, %g], initial peak position: %g\n",
              fitLow, fitHigh, peak0);
  std::printf("Requested mode: auto\n");
  std::printf("ROOT fit option: RQSN\n");
  std::printf("TSpectrum background option: none\n");
  std::printf("Relative position fixed: false\n");
  std::printf("Relative FWHM fixed: false\n");
  std::printf("Fitting function: %s\n", result.model.c_str());
  std::printf("Fit bins = %d, free parameters = %d\n",
              result.fitBins, result.freeParameters);
  std::printf("Fit status: %d\n", result.status);
  if(result.ndf > 0) {
    std::printf("chi2 = %.10g, ndf = %d, reduced chisq = %.10g\n",
                result.chi2, result.ndf, result.reducedChi2);
  } else {
    std::printf("chi2 = %.10g, ndf = %d, reduced chisq = n/a\n",
                result.chi2, result.ndf);
  }
  std::printf("Photopeak area = %.10g +/- %.10g\n",
              result.area, result.areaError);
  std::printf("\nParameters:\n");
  for(int index : order) {
    std::printf("  %-5s = % .10g +/- %.10g\n", names[index],
                result.parameters[index], result.errors[index]);
  }
  std::printf("\n");
  std::fflush(stdout);
}

} // namespace

// ============== PhotoPeakFitter::Fit ==============
// Purpose: Run the minimal default single-photopeak auto fit.
// Inputs: Histogram, fit limits, initial centroid, optional pad, and draw flag.
// Outputs: Structured fit result with terminal report and optional curves.
PhotoPeakFitResult PhotoPeakFitter::Fit(TH1* hist,
                                        double fitLow,
                                        double fitHigh,
                                        double peak0,
                                        TVirtualPad* pad,
                                        bool draw) {
  PhotoPeakFitResult output;
  if(!hist) {
    std::printf("photopeakfit ERROR: null histogram pointer.\n");
    return output;
  }
  if(hist->GetDimension() != 1) {
    std::printf("photopeakfit ERROR: only one-dimensional histograms are supported.\n");
    return output;
  }
  if(fitLow == fitHigh) {
    std::printf("photopeakfit ERROR: fitLow and fitHigh are equal.\n");
    output.status = 2;
    return output;
  }
  if(fitLow > fitHigh)
    std::swap(fitLow, fitHigh);

  const double lowContent = BinContentAt(hist, fitLow);
  const double highContent = BinContentAt(hist, fitHigh);
  const double peakContent = BinContentAt(hist, peak0);
  const double midpoint = 0.5 * (fitLow + fitHigh);
  const double range = fitHigh - fitLow;
  const double a0 = 0.5 * (lowContent + highContent);
  const double b0 = (highContent - lowContent) / range;
  const double c0 = 0.0;
  const double r0 = 10.0;
  const double fwhm0 = std::sqrt(std::max(9.0 + 0.004 * peak0, 1.0e-12));
  const double beta0 = 0.5 * fwhm0;
  const double step0 = 0.25;
  const double linearBackground = a0 + b0 * (peak0 - midpoint);
  const double maximum = MaximumInRange(hist, fitLow, fitHigh);
  const double height0 = std::max(peakContent - linearBackground,
                                  std::max(maximum, 1.0));
  const double heightUpper = std::max(10.0 * maximum, height0 * 10.0);
  const int fitBins = BinCountInRange(hist, fitLow, fitHigh);

  const Candidate candidates[] = {
    {"gaussian_linearBg", false, false, false},
    {"gaussian_linearBg_tail", true, false, false},
    {"gaussian_linearBg_step", false, true, false},
    {"gaussian_linearBg_quadBg", false, false, true},
    {"gaussian_linearBg_tail_step", true, true, false},
    {"gaussian_linearBg_tail_quadBg", true, false, true},
    {"gaussian_linearBg_step_quadBg", false, true, true},
    {"gaussian_linearBg_tail_step_quadBg", true, true, true}
  };

  std::vector<Trial> trials;
  trials.reserve(8);
  Trial* best = nullptr;
  Trial* fallback = nullptr;
  for(int index = 0; index < 8; ++index) {
    trials.emplace_back();
    Trial& trial = trials.back();
    trial.candidate = candidates[index];
    trial.freeParameters = FreeParameterCount(trial.candidate);
    const bool basic = !trial.candidate.useTail &&
      !trial.candidate.useStep && !trial.candidate.useQuadBg;
    if(!basic && fitBins <= trial.freeParameters)
      continue;

    const TString functionName = TString::Format(
      "PhotoPeakTrial_%p_%s", static_cast<void*>(hist), trial.candidate.name);
    trial.function = new TF1(functionName.Data(), PhotoPeakEvaluator(midpoint),
                             fitLow, fitHigh, kPhotoPeakNPars);
    ConfigureFunction(trial.function, trial.candidate,
                      a0, b0, c0, r0, beta0, step0,
                      peak0, fwhm0, height0,
                      fitLow, fitHigh, range, heightUpper);
    TFitResultPtr fitResult = hist->Fit(trial.function, "RQSN");
    trial.status = static_cast<int>(fitResult);
    trial.chi2 = trial.function->GetChisquare();
    trial.ndf = trial.function->GetNDF();
    trial.reducedChi2 = trial.ndf > 0 ? trial.chi2 / trial.ndf :
      std::numeric_limits<double>::quiet_NaN();
    if(fitResult.Get() && fitResult->CovMatrixStatus() > 0)
      trial.covariance = fitResult->GetCovarianceMatrix();
    if(basic)
      fallback = &trial;
    if(TrialIsBetter(&trial, best))
      best = &trial;
  }

  if(!best)
    best = fallback;
  if(!best || !best->function) {
    std::printf("photopeakfit ERROR: no fitting function was available.\n");
    output.status = 3;
    for(auto& trial : trials)
      delete trial.function;
    return output;
  }

  for(auto& trial : trials) {
    if(&trial != best) {
      delete trial.function;
      trial.function = nullptr;
    }
  }

  TF1* total = best->function;
  output.status = best->status;
  output.model = best->candidate.name;
  output.fitBins = fitBins;
  output.freeParameters = best->freeParameters;
  output.ndf = best->ndf;
  output.chi2 = best->chi2;
  output.reducedChi2 = best->reducedChi2;
  for(int index = 0; index < kPhotoPeakNPars; ++index) {
    output.parameters[index] = total->GetParameter(index);
    output.errors[index] = total->GetParError(index);
  }
  const int centroidBin = hist->GetXaxis()->FindFixBin(
    output.parameters[kPhotoPeakPosition]);
  const double binWidth = hist->GetXaxis()->GetBinWidth(centroidBin);
  output.area = PeakArea(output.parameters.data(), binWidth);
  output.areaError = PeakAreaUncertainty(output.parameters.data(),
                                         best->covariance, binWidth);
  PrintResult(hist, fitLow, fitHigh, peak0, output);

  if(draw) {
    TVirtualPad* targetPad = pad ? pad : gPad;
    if(!targetPad) {
      targetPad = new TCanvas("PhotoPeak_canvas", "PhotoPeak_canvas", 900, 650);
      hist->Draw();
    } else {
      targetPad->cd();
    }
    RemovePreviousDrawObjects(targetPad, hist);

    total->SetName(TString::Format("PhotoPeak_%p_total_fit",
                                   static_cast<void*>(hist)).Data());
    total->SetLineColor(kRed);
    total->SetLineStyle(1);
    total->SetLineWidth(3);
    total->SetNpx(2000);

    auto* background = new TF1(
      TString::Format("PhotoPeak_%p_background_fit",
                      static_cast<void*>(hist)).Data(),
      PhotoPeakBackgroundEvaluator(midpoint), fitLow, fitHigh,
      kPhotoPeakNPars);
    for(int index = 0; index < kPhotoPeakNPars; ++index)
      background->SetParameter(index, output.parameters[index]);
    background->SetLineColor(kBlack);
    background->SetLineStyle(2);
    background->SetLineWidth(3);
    background->SetNpx(2000);

    total->Draw("same");
    background->Draw("same");
    targetPad->Modified();
    targetPad->Update();
  } else {
    delete total;
  }

  return output;
}

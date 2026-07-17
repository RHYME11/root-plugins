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
#include <TLegend.h>
#include <TLatex.h>
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
                       double range, double heightUpper,
                       const PhotoPeakFitConfig& config) {
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
  for(int index = 0; index < kPhotoPeakNPars; ++index) {
    if((index == kPhotoPeakR || index == kPhotoPeakBeta) && !candidate.useTail)
      continue;
    if(index == kPhotoPeakStep && !candidate.useStep)
      continue;
    if(index == kPhotoPeakC && !candidate.useQuadBg)
      continue;
    const auto& control = config.global[index];
    if(control.mode == PhotoPeakParameterMode::Fixed) {
      function->FixParameter(index, control.value);
    } else if(control.mode == PhotoPeakParameterMode::Limited) {
      if(control.value != 0.0)
        function->SetParameter(index, control.value);
      function->SetParLimits(index, std::min(control.lower, control.upper),
                             std::max(control.lower, control.upper));
    } else if(control.value != 0.0) {
      function->SetParameter(index, control.value);
    }
  }
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
                 const PhotoPeakFitConfig& config,
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
  const char* mode = config.mode == PhotoPeakFitMode::Auto ? "auto" :
    config.mode == PhotoPeakFitMode::HighStat ? "highstat" : "lowstat";
  std::printf("Requested mode: %s\n", mode);
  std::printf("ROOT fit option: %s\n", config.rootOptions.c_str());
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
PhotoPeakFitResult PhotoPeakFitter::Fit(TH1* hist, double fitLow,
                                        double fitHigh, double peak0,
                                        TVirtualPad* pad, bool draw) {
  return Fit(hist, fitLow, fitHigh, peak0, PhotoPeakFitConfig::Defaults(),
             pad, draw);
}

PhotoPeakFitResult PhotoPeakFitter::Fit(TH1* hist,
                                        double fitLow,
                                        double fitHigh,
                                        double peak0,
                                        const PhotoPeakFitConfig& requestedConfig,
                                        TVirtualPad* pad,
                                        bool draw) {
  PhotoPeakFitResult output;
  PhotoPeakFitConfig config = requestedConfig;
  config.Normalize(1);
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
    const bool selected = config.mode == PhotoPeakFitMode::Auto ||
      (config.mode == PhotoPeakFitMode::HighStat && index == 7) ||
      (config.mode == PhotoPeakFitMode::LowStat && index == 0);
    if(!selected)
      continue;
    if(!basic && fitBins <= trial.freeParameters)
      continue;

    const TString functionName = TString::Format(
      "PhotoPeakTrial_%p_%s", static_cast<void*>(hist), trial.candidate.name);
    trial.function = new TF1(functionName.Data(), PhotoPeakEvaluator(midpoint),
                             fitLow, fitHigh, kPhotoPeakNPars);
    ConfigureFunction(trial.function, trial.candidate,
                      a0, b0, c0, r0, beta0, step0,
                      peak0, fwhm0, height0,
                      fitLow, fitHigh, range, heightUpper, config);
    std::string options = config.rootOptions;
    for(char required : std::string("RSN")) {
      if(options.find(required) == std::string::npos)
        options.push_back(required);
    }
    TFitResultPtr fitResult = hist->Fit(trial.function, options.c_str());
    trial.status = static_cast<int>(fitResult);
    trial.chi2 = trial.function->GetChisquare();
    trial.ndf = trial.function->GetNDF();
    trial.reducedChi2 = trial.ndf > 0 ? trial.chi2 / trial.ndf :
      std::numeric_limits<double>::quiet_NaN();
    if(fitResult.Get() && fitResult->CovMatrixStatus() > 0)
      trial.covariance = fitResult->GetCovarianceMatrix();
    if(basic)
      fallback = &trial;
    if(config.mode == PhotoPeakFitMode::Auto) {
      if(TrialIsBetter(&trial, best))
        best = &trial;
    } else {
      best = &trial;
    }
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
  PrintResult(hist, fitLow, fitHigh, peak0, config, output);

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

    auto* legend = new TLegend(0.68, 0.76, 0.93, 0.9);
    legend->SetName(TString::Format("PhotoPeak_%p_legend",
                                   static_cast<void*>(hist)).Data());
    legend->AddEntry(total, "total fit", "l");
    legend->AddEntry(background, "background", "l");
    legend->Draw();

    auto* label = new TLatex(output.parameters[kPhotoPeakPosition],
      hist->GetMaximum() * 0.76,
      TString::Format("%.3f", output.parameters[kPhotoPeakPosition]).Data());
    label->SetName(TString::Format("PhotoPeak_%p_centroid_0",
                                  static_cast<void*>(hist)).Data());
    label->SetTextAngle(90.0);
    label->SetTextColor(kRed);
    label->SetTextSize(0.03);
    label->Draw();
    targetPad->Modified();
    targetPad->Update();
  } else {
    delete total;
  }

  return output;
}

// ============== PhotoPeakFitter::Fit ==============
// Purpose: Fit a structured single- or multi-peak request.
// Inputs: Histogram, request, optional pad, and draw flag.
// Outputs: Structured fit and per-peak results.
PhotoPeakFitResult PhotoPeakFitter::Fit(TH1* hist,
                                        const PhotoPeakFitRequest& request,
                                        TVirtualPad* pad,
                                        bool draw) {
  if(!hist || request.peaks.empty())
    return PhotoPeakFitResult{};
  if(request.peaks.size() == 1) {
    PhotoPeakFitConfig config = request.config;
    config.global[kPhotoPeakPosition] = request.peaks.front().centroid;
    config.global[kPhotoPeakFwhm] = request.peaks.front().fwhm;
    config.global[kPhotoPeakHeight] = request.peaks.front().height;
    PhotoPeakFitResult result = Fit(
      hist, request.fitLow, request.fitHigh,
      request.peaks.front().centroid.value, config, pad, draw);
    PhotoPeakFitResult::Peak peak;
    peak.centroid = result.parameters[kPhotoPeakPosition];
    peak.centroidError = result.errors[kPhotoPeakPosition];
    peak.height = result.parameters[kPhotoPeakHeight];
    peak.heightError = result.errors[kPhotoPeakHeight];
    peak.fwhm = result.parameters[kPhotoPeakFwhm];
    peak.fwhmError = result.errors[kPhotoPeakFwhm];
    peak.area = result.area;
    peak.areaError = result.areaError;
    result.peaks.push_back(peak);
    return result;
  }

  PhotoPeakFitResult output;
  const double low = std::min(request.fitLow, request.fitHigh);
  const double high = std::max(request.fitLow, request.fitHigh);
  const double midpoint = 0.5 * (low + high);
  const int peakCount = static_cast<int>(request.peaks.size());
  PhotoPeakFitConfig config = request.config;
  config.Normalize(request.peaks.size());
  const int widthScaleIndex = 6;
  const int peakBase = 7;
  const int parameterCount = peakBase + 3 * peakCount;
  const double referencePosition = request.peaks.front().centroid.value;
  std::vector<double> positionOffsets;
  std::vector<double> referenceWidths;
  for(const auto& seed : request.peaks) {
    positionOffsets.push_back(seed.centroid.value - referencePosition);
    referenceWidths.push_back(seed.fwhm.value > 0.0 ? seed.fwhm.value :
      std::sqrt(std::max(9.0 + 0.004 * seed.centroid.value, 1.0e-12)));
  }
  struct MultiCandidate {
    const char* name;
    bool tail;
    bool step;
    bool quadratic;
  };
  const MultiCandidate candidates[] = {
    {"multi_gaussian_linearBg", false, false, false},
    {"multi_gaussian_linearBg_tail", true, false, false},
    {"multi_gaussian_linearBg_step", false, true, false},
    {"multi_gaussian_linearBg_quadBg", false, false, true},
    {"multi_gaussian_linearBg_tail_step", true, true, false},
    {"multi_gaussian_linearBg_tail_quadBg", true, false, true},
    {"multi_gaussian_linearBg_step_quadBg", false, true, true},
    {"multi_gaussian_linearBg_tail_step_quadBg", true, true, true}
  };
  const double lowValue = BinContentAt(hist, low);
  const double highValue = BinContentAt(hist, high);
  std::string options = request.config.rootOptions;
  for(char required : std::string("RSN")) {
    if(options.find(required) == std::string::npos)
      options.push_back(required);
  }
  TF1* total = nullptr;
  TMatrixDSym bestCovariance(parameterCount);
  MultiCandidate bestCandidate{"", false, false, false};
  double bestReduced = std::numeric_limits<double>::infinity();
  int bestStatus = 1;
  for(int candidateIndex = 0; candidateIndex < 8; ++candidateIndex) {
    const auto candidate = candidates[candidateIndex];
    const bool selected = config.mode == PhotoPeakFitMode::Auto ||
      (config.mode == PhotoPeakFitMode::HighStat && candidateIndex == 7) ||
      (config.mode == PhotoPeakFitMode::LowStat && candidateIndex == 0);
    if(!selected)
      continue;
    auto evaluator = [midpoint, peakCount, candidate, config,
                      positionOffsets, referenceWidths, peakBase,
                      widthScaleIndex](double* x, double* par) {
      const double centered = x[0] - midpoint;
      double value = par[kPhotoPeakA] + par[kPhotoPeakB] * centered +
        par[kPhotoPeakC] * centered * centered;
      for(int index = 0; index < peakCount; ++index) {
        const int offset = peakBase + 3 * index;
        const double position = config.relativePosition ?
          par[peakBase] + positionOffsets[index] : par[offset];
        const double fwhm = std::max(config.relativeFwhm ?
          par[widthScaleIndex] * referenceWidths[index] : par[offset + 1],
          1.0e-12);
        const double height = par[offset + 2];
        const double beta = std::max(par[kPhotoPeakBeta], 1.0e-12);
        const double sigma = fwhm / 2.35482;
        const double w = (x[0] - position) / (sigma * TMath::Sqrt2());
        const double y = fwhm / (beta * 3.33021838);
        const double gaussian = std::exp(-w * w);
        double skew = 0.0;
        if(candidate.tail) {
          const double argument = (x[0] - position) / beta;
          if(std::fabs(argument) <= 700.0)
            skew = std::exp(argument) * TMath::Erfc(w + y) /
              std::max(TMath::Erfc(y), 1.0e-300);
        }
        const double fraction = par[kPhotoPeakR] / 100.0;
        value += height * ((1.0 - fraction) * gaussian + fraction * skew);
        if(candidate.step)
          value += height * par[kPhotoPeakStep] * TMath::Erfc(w) / 200.0;
      }
      return value;
    };
    auto* trial = new TF1(TString::Format("PhotoPeakTrial_%p_multi_%d",
      static_cast<void*>(hist), candidateIndex).Data(), evaluator,
      low, high, parameterCount);
    trial->SetParameters(0.5 * (lowValue + highValue),
      (highValue - lowValue) / std::max(high - low, 1.0e-12),
      0.0, 10.0, 0.5 * referenceWidths.front(), 0.25);
    trial->SetParameter(widthScaleIndex, 1.0);
    trial->SetParLimits(kPhotoPeakR, 0.0, 100.0);
    trial->SetParLimits(kPhotoPeakBeta, 1.0e-6, 10.0 * (high - low));
    trial->SetParLimits(kPhotoPeakStep, 0.0, 100.0);
    if(!candidate.tail) {
      trial->FixParameter(kPhotoPeakR, 0.0);
      trial->FixParameter(kPhotoPeakBeta, 0.5 * referenceWidths.front());
    }
    if(!candidate.step)
      trial->FixParameter(kPhotoPeakStep, 0.0);
    if(!candidate.quadratic)
      trial->FixParameter(kPhotoPeakC, 0.0);
    for(int parameter = 0; parameter <= kPhotoPeakStep; ++parameter) {
      if((parameter == kPhotoPeakR || parameter == kPhotoPeakBeta) &&
         !candidate.tail)
        continue;
      if(parameter == kPhotoPeakStep && !candidate.step)
        continue;
      if(parameter == kPhotoPeakC && !candidate.quadratic)
        continue;
      const auto& control = config.global[parameter];
      if(control.mode == PhotoPeakParameterMode::Fixed)
        trial->FixParameter(parameter, control.value);
      else if(control.mode == PhotoPeakParameterMode::Limited) {
        if(control.value != 0.0)
          trial->SetParameter(parameter, control.value);
        trial->SetParLimits(parameter, std::min(control.lower, control.upper),
                           std::max(control.lower, control.upper));
      } else if(control.value != 0.0) {
        trial->SetParameter(parameter, control.value);
      }
    }
    if(config.relativeFwhm) {
      trial->SetParLimits(widthScaleIndex, config.widthScale.lower,
                         config.widthScale.upper);
      if(config.widthScale.mode == PhotoPeakParameterMode::Fixed)
        trial->FixParameter(widthScaleIndex, config.widthScale.value);
    } else {
      trial->FixParameter(widthScaleIndex, 1.0);
    }
    for(int index = 0; index < peakCount; ++index) {
      const auto& seed = request.peaks[index];
      const int offset = peakBase + 3 * index;
      trial->SetParameter(offset, seed.centroid.value);
      trial->SetParameter(offset + 1, referenceWidths[index]);
      trial->SetParameter(offset + 2, seed.height.value > 0.0 ? seed.height.value :
        std::max(BinContentAt(hist, seed.centroid.value) - trial->GetParameter(0), 1.0));
      trial->SetParLimits(offset, low, high);
      trial->SetParLimits(offset + 1, 1.0e-6, high - low);
      trial->SetParLimits(offset + 2, 0.0, 10.0 * MaximumInRange(hist, low, high));
      if(config.relativePosition && index > 0)
        trial->FixParameter(offset, seed.centroid.value);
      if(config.relativeFwhm)
        trial->FixParameter(offset + 1, referenceWidths[index]);
      const PhotoPeakParameterControl controls[3] = {
        seed.centroid, seed.fwhm, seed.height};
      for(int item = 0; item < 3; ++item) {
        if((config.relativePosition && index > 0 && item == 0) ||
           (config.relativeFwhm && item == 1))
          continue;
        const int parameter = offset + item;
        if(controls[item].mode == PhotoPeakParameterMode::Fixed)
          trial->FixParameter(parameter, controls[item].value);
        else if(controls[item].mode == PhotoPeakParameterMode::Limited)
          trial->SetParLimits(parameter, controls[item].lower,
                             controls[item].upper);
      }
    }
    TFitResultPtr fitResult = hist->Fit(trial, options.c_str());
    const int status = static_cast<int>(fitResult);
    const double reduced = trial->GetNDF() > 0 ?
      trial->GetChisquare() / trial->GetNDF() :
      std::numeric_limits<double>::infinity();
    const bool better = !total || (status == 0 && bestStatus != 0) ||
      (status == bestStatus && reduced < bestReduced);
    if(better) {
      delete total;
      total = trial;
      bestStatus = status;
      bestReduced = reduced;
      output.model = candidate.name;
      bestCandidate = candidate;
      bestCovariance.Zero();
      if(fitResult.Get() && fitResult->CovMatrixStatus() > 0)
        bestCovariance = fitResult->GetCovarianceMatrix();
    } else {
      delete trial;
    }
  }
  if(!total)
    return output;
  total->SetName(TString::Format("PhotoPeak_%p_total_fit",
                                 static_cast<void*>(hist)).Data());
  output.status = bestStatus;
  output.chi2 = total->GetChisquare();
  output.ndf = total->GetNDF();
  output.reducedChi2 = output.ndf > 0 ? output.chi2 / output.ndf : 0.0;
  output.fitBins = BinCountInRange(hist, low, high);
  output.freeParameters = parameterCount;
  std::vector<double> fittedParameters(parameterCount);
  for(int parameter = 0; parameter < parameterCount; ++parameter)
    fittedParameters[parameter] = total->GetParameter(parameter);
  auto peakAreaAt = [&](int peakIndex, const std::vector<double>& parameters) {
    const int offset = peakBase + 3 * peakIndex;
    const double centroid = config.relativePosition ?
      parameters[peakBase] + positionOffsets[peakIndex] : parameters[offset];
    double areaParameters[kPhotoPeakNPars] = {0.0};
    areaParameters[kPhotoPeakR] = parameters[kPhotoPeakR];
    areaParameters[kPhotoPeakBeta] = parameters[kPhotoPeakBeta];
    areaParameters[kPhotoPeakFwhm] = config.relativeFwhm ?
      parameters[widthScaleIndex] * referenceWidths[peakIndex] :
      parameters[offset + 1];
    areaParameters[kPhotoPeakHeight] = parameters[offset + 2];
    return PeakArea(areaParameters, hist->GetXaxis()->GetBinWidth(
      hist->GetXaxis()->FindFixBin(centroid)));
  };
  std::vector<double> totalAreaGradient(parameterCount, 0.0);
  for(int index = 0; index < peakCount; ++index) {
    const int offset = peakBase + 3 * index;
    PhotoPeakFitResult::Peak peak;
    peak.centroid = config.relativePosition ? total->GetParameter(peakBase) +
      positionOffsets[index] : total->GetParameter(offset);
    peak.centroidError = config.relativePosition ? total->GetParError(peakBase) :
      total->GetParError(offset);
    peak.fwhm = config.relativeFwhm ? total->GetParameter(widthScaleIndex) *
      referenceWidths[index] : total->GetParameter(offset + 1);
    peak.fwhmError = config.relativeFwhm ? total->GetParError(widthScaleIndex) *
      referenceWidths[index] : total->GetParError(offset + 1);
    peak.height = total->GetParameter(offset + 2);
    peak.heightError = total->GetParError(offset + 2);
    peak.area = peakAreaAt(index, fittedParameters);
    std::vector<double> gradient(parameterCount, 0.0);
    for(int parameter = 0; parameter < parameterCount; ++parameter) {
      const double step = std::sqrt(std::numeric_limits<double>::epsilon()) *
        std::max(std::fabs(fittedParameters[parameter]), 1.0);
      auto upper = fittedParameters;
      auto lowerParameters = fittedParameters;
      upper[parameter] += step;
      lowerParameters[parameter] -= step;
      gradient[parameter] = (peakAreaAt(index, upper) -
        peakAreaAt(index, lowerParameters)) / (2.0 * step);
      totalAreaGradient[parameter] += gradient[parameter];
    }
    double areaVariance = 0.0;
    for(int row = 0; row < parameterCount; ++row) {
      for(int column = 0; column < parameterCount; ++column) {
        areaVariance += gradient[row] * bestCovariance(row, column) *
          gradient[column];
      }
    }
    peak.areaError = areaVariance > 0.0 ? std::sqrt(areaVariance) : 0.0;
    output.area += peak.area;
    output.peaks.push_back(peak);
  }
  double totalAreaVariance = 0.0;
  for(int row = 0; row < parameterCount; ++row) {
    for(int column = 0; column < parameterCount; ++column) {
      totalAreaVariance += totalAreaGradient[row] *
        bestCovariance(row, column) * totalAreaGradient[column];
    }
  }
  output.areaError = totalAreaVariance > 0.0 ?
    std::sqrt(totalAreaVariance) : 0.0;
  std::printf("\nmultipeakfit result for %s\n", hist->GetName());
  std::printf("Fitting function: %s\nFit status: %d\n", output.model.c_str(),
              output.status);
  for(std::size_t index = 0; index < output.peaks.size(); ++index) {
    const auto& peak = output.peaks[index];
    std::printf("  Peak %zu: P = %.10g +/- %.10g, W = %.10g +/- %.10g, "
                "Area = %.10g +/- %.10g\n", index + 1, peak.centroid,
                peak.centroidError, peak.fwhm, peak.fwhmError, peak.area,
                peak.areaError);
  }

  if(draw) {
    TVirtualPad* targetPad = pad ? pad : gPad;
    if(targetPad) {
      targetPad->cd();
      RemovePreviousDrawObjects(targetPad, hist);
      total->SetLineColor(kRed);
      total->SetLineWidth(3);
      total->Draw("same");
      auto* legend = new TLegend(0.68, 0.72, 0.93, 0.9);
      legend->SetName(TString::Format("PhotoPeak_%p_legend",
                                     static_cast<void*>(hist)).Data());
      legend->AddEntry(total, "total fit", "l");
      auto backgroundEvaluator = [midpoint, peakCount, bestCandidate,
                                  fittedParameters, config, positionOffsets,
                                  referenceWidths, peakBase, widthScaleIndex]
        (double* x, double*) {
          const double centered = x[0] - midpoint;
          double value = fittedParameters[kPhotoPeakA] +
            fittedParameters[kPhotoPeakB] * centered +
            fittedParameters[kPhotoPeakC] * centered * centered;
          if(bestCandidate.step) {
            for(int peakIndex = 0; peakIndex < peakCount; ++peakIndex) {
              const int offset = peakBase + 3 * peakIndex;
              const double position = config.relativePosition ?
                fittedParameters[peakBase] + positionOffsets[peakIndex] :
                fittedParameters[offset];
              const double fwhm = config.relativeFwhm ?
                fittedParameters[widthScaleIndex] * referenceWidths[peakIndex] :
                fittedParameters[offset + 1];
              const double sigma = std::max(fwhm, 1.0e-12) / 2.35482;
              const double w = (x[0] - position) /
                (sigma * TMath::Sqrt2());
              value += fittedParameters[offset + 2] *
                fittedParameters[kPhotoPeakStep] * TMath::Erfc(w) / 200.0;
            }
          }
          return value;
        };
      auto* background = new TF1(TString::Format(
        "PhotoPeak_%p_background_fit", static_cast<void*>(hist)).Data(),
        backgroundEvaluator, low, high, 0);
      background->SetLineColor(kBlack);
      background->SetLineStyle(2);
      background->SetLineWidth(3);
      background->SetNpx(2000);
      background->Draw("same");
      legend->AddEntry(background, "background", "l");
      for(std::size_t index = 0; index < output.peaks.size(); ++index) {
        const auto& peak = output.peaks[index];
        const int color = kBlue + static_cast<int>(index % 4);
        auto componentEvaluator = [index, bestCandidate, fittedParameters,
                                   config, positionOffsets, referenceWidths,
                                   peakBase, widthScaleIndex]
          (double* x, double*) {
            const int offset = peakBase + 3 * static_cast<int>(index);
            const double position = config.relativePosition ?
              fittedParameters[peakBase] + positionOffsets[index] :
              fittedParameters[offset];
            const double fwhm = std::max(config.relativeFwhm ?
              fittedParameters[widthScaleIndex] * referenceWidths[index] :
              fittedParameters[offset + 1], 1.0e-12);
            const double sigma = fwhm / 2.35482;
            const double w = (x[0] - position) /
              (sigma * TMath::Sqrt2());
            const double gaussian = std::exp(-w * w);
            double skew = 0.0;
            if(bestCandidate.tail) {
              const double beta = std::max(
                fittedParameters[kPhotoPeakBeta], 1.0e-12);
              const double y = fwhm / (beta * 3.33021838);
              const double argument = (x[0] - position) / beta;
              if(std::fabs(argument) <= 700.0) {
                skew = std::exp(argument) * TMath::Erfc(w + y) /
                  std::max(TMath::Erfc(y), 1.0e-300);
              }
            }
            const double fraction = fittedParameters[kPhotoPeakR] / 100.0;
            return fittedParameters[offset + 2] *
              ((1.0 - fraction) * gaussian + fraction * skew);
          };
        auto* component = new TF1(TString::Format(
          "PhotoPeak_%p_component_%zu", static_cast<void*>(hist), index).Data(),
          componentEvaluator, low, high, 0);
        component->SetLineColor(color);
        component->SetLineWidth(2);
        component->SetNpx(2000);
        component->Draw("same");
        legend->AddEntry(component,
          TString::Format("peak %zu", index + 1).Data(), "l");
        auto* label = new TLatex(peak.centroid,
          hist->GetMaximum() * (0.72 + 0.04 * (index % 4)),
          TString::Format("%.3f", peak.centroid).Data());
        label->SetName(TString::Format("PhotoPeak_%p_centroid_%zu",
                                      static_cast<void*>(hist), index).Data());
        label->SetTextAngle(90.0);
        label->SetTextColor(color);
        label->SetTextSize(0.03);
        label->Draw();
      }
      legend->Draw();
      targetPad->Modified();
      targetPad->Update();
    }
  } else {
    delete total;
  }
  return output;
}

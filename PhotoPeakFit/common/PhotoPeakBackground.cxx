#include "PhotoPeakBackground.h"

#include <algorithm>

#include <TF1.h>
#include <TH1.h>
#include <TList.h>
#include <TSpectrum.h>
#include <TString.h>
#include <TVirtualFitter.h>
#include <TVirtualPad.h>

// ============== EstimatePhotoPeakBackground ==============
// Purpose: Estimate the selected background without changing the source or zoom.
// Inputs: Original histogram, settings and fit bounds. Outputs: Detached estimate or error.
PhotoPeakBackgroundEstimate EstimatePhotoPeakBackground(TH1* histogram,
  const PhotoPeakFitConfig& config, double fitLow, double fitHigh) {
  PhotoPeakBackgroundEstimate estimate;
  if(config.background == PhotoPeakBackgroundMode::None)
    return estimate;
  if(config.rootOptions.find_first_of("Ll") != std::string::npos) {
    estimate.error = "Background-subtracted fits require chi-square fitting. Remove L from ROOT fit options.";
    return estimate;
  }
  if(!histogram || histogram->GetDimension() != 1) {
    estimate.error = "Background requires a one-dimensional histogram.";
    return estimate;
  }
  std::unique_ptr<TH1> source(static_cast<TH1*>(histogram->Clone("PhotoPeak_background_source")));
  source->SetDirectory(nullptr);
  source->GetXaxis()->SetRange(1, source->GetNbinsX());
  if(config.background == PhotoPeakBackgroundMode::Local) {
    const double low = std::max(source->GetXaxis()->GetXmin(),
      std::min(config.backgroundLow, std::min(fitLow, fitHigh)));
    const double high = std::min(source->GetXaxis()->GetXmax(),
      std::max(config.backgroundHigh, std::max(fitLow, fitHigh)));
    source->GetXaxis()->SetRangeUser(low, high);
  }
  const auto* axis = source->GetXaxis();
  estimate.low = axis->GetBinLowEdge(axis->GetFirst());
  estimate.high = axis->GetBinUpEdge(axis->GetLast());
  const int bins = axis->GetLast() - axis->GetFirst() + 1;
  if(config.backgroundIterations < 1 || config.backgroundIterations > (bins - 1) / 2) {
    estimate.error = "Background iterations: selected range needs at least 2 * iterations + 1 bins.";
    return estimate;
  }
  TSpectrum spectrum;
  estimate.histogram.reset(spectrum.Background(source.get(), config.backgroundIterations,
    config.backgroundOptions.c_str()));
  if(!estimate.histogram) {
    estimate.error = "TSpectrum could not estimate the background. Adjust its settings before fitting.";
    return estimate;
  }
  estimate.histogram->SetDirectory(nullptr);
  return estimate;
}

// ============== PhotoPeakBackgroundAt ==============
// Purpose: Read the estimated baseline on the original bin grid.
// Inputs: Detached estimate and x coordinate. Outputs: Background bin content or zero.
double PhotoPeakBackgroundAt(const PhotoPeakBackgroundEstimate& estimate, double x) {
  if(!estimate.histogram)
    return 0.0;
  const auto* histogram = estimate.histogram.get();
  const int bin = std::clamp(histogram->GetXaxis()->FindFixBin(x), 1, histogram->GetNbinsX());
  return histogram->GetBinContent(bin);
}

// ============== RemovePhotoPeakBackground ==============
// Purpose: Remove only the background estimate owned by this target.
// Inputs: Pad and histogram identity. Outputs: Removed estimate drawing.
void RemovePhotoPeakBackground(TVirtualPad* pad, TH1* histogram) {
  if(!pad || !pad->GetListOfPrimitives())
    return;
  const TString name = TString::Format("PhotoPeakBackground_%p_estimate", static_cast<void*>(histogram));
  if(auto* object = pad->GetListOfPrimitives()->FindObject(name)) {
    pad->GetListOfPrimitives()->Remove(object);
    delete object;
  }
}

// ============== DrawPhotoPeakBackground ==============
// Purpose: Show a visible background estimate on the original spectrum.
// Inputs: Target pad/histogram and estimate. Outputs: Thick green dashed baseline, with no duplicate.
void DrawPhotoPeakBackground(TVirtualPad* pad, TH1* histogram,
  const PhotoPeakBackgroundEstimate& estimate) {
  RemovePhotoPeakBackground(pad, histogram);
  if(!pad || !estimate.histogram)
    return;
  TVirtualPad::TContext context(pad);
  auto* drawing = static_cast<TH1*>(estimate.histogram->Clone(TString::Format(
    "PhotoPeakBackground_%p_estimate", static_cast<void*>(histogram))));
  drawing->SetDirectory(nullptr);
  drawing->SetStats(false);
  drawing->SetLineColor(kGreen + 2);
  drawing->SetLineWidth(3);
  drawing->SetLineStyle(7);
  drawing->Draw("hist same");
}

// ============== RestorePhotoPeakBackground ==============
// Purpose: Lift a residual fit curve back onto the original spectrum.
// Inputs: Owned residual function and shared estimate. Outputs: Display function owning both inputs.
TF1* RestorePhotoPeakBackground(TF1* residual, const PhotoPeakBackgroundEstimate& estimate) {
  if(!estimate.histogram)
    return residual;
  residual->AddToGlobalList(false);
  std::shared_ptr<TF1> fitted(residual);
  auto evaluator = [fitted, estimate](double* x, double* parameters) {
    return fitted->EvalPar(x, parameters) + PhotoPeakBackgroundAt(estimate, x[0]);
  };
  auto* display = new TF1((std::string(residual->GetName()) + "_display").c_str(),
    evaluator, residual->GetXmin(), residual->GetXmax(), residual->GetNpar());
  display->SetParameters(residual->GetParameters());
  for(int parameter = 0; parameter < residual->GetNpar(); ++parameter) {
    double low = 0.0, high = 0.0;
    residual->GetParLimits(parameter, low, high);
    display->SetParLimits(parameter, low, high);
    display->SetParName(parameter, residual->GetParName(parameter));
  }
  display->SetParErrors(residual->GetParErrors());
  display->SetNumberFitPoints(residual->GetNumberFitPoints());
  display->SetChisquare(residual->GetChisquare());
  display->SetNDF(residual->GetNDF());
  display->SetNpx(residual->GetNpx());
  return display;
}

// ============== PhotoPeakFitInput::PhotoPeakFitInput ==============
// Purpose: Prepare background-subtracted data while retaining the original statistical errors.
// Inputs: Original histogram, settings and fit bounds. Outputs: Original or detached fit input.
PhotoPeakFitInput::PhotoPeakFitInput(TH1* original, const PhotoPeakFitConfig& config,
  double fitLow, double fitHigh)
  : histogram(original), background(EstimatePhotoPeakBackground(original, config, fitLow, fitHigh)) {
  if(!background.histogram)
    return;
  fSubtracted.reset(static_cast<TH1*>(original->Clone("PhotoPeak_subtracted_input")));
  fSubtracted->SetDirectory(nullptr);
  if(!fSubtracted->GetSumw2N())
    fSubtracted->Sumw2();
  for(int bin = 1; bin <= original->GetNbinsX(); ++bin) {
    fSubtracted->SetBinContent(bin, original->GetBinContent(bin) - background.histogram->GetBinContent(bin));
    fSubtracted->SetBinError(bin, original->GetBinError(bin));
  }
  fSubtracted->SetEntries(original->GetEntries());
  histogram = fSubtracted.get();
}

// ============== PhotoPeakFitInput::~PhotoPeakFitInput ==============
// Purpose: Release a retained ROOT fitter before its temporary input is destroyed.
// Inputs: Owned fit input. Outputs: No dangling ROOT fitter reference.
PhotoPeakFitInput::~PhotoPeakFitInput() {
  if(!fSubtracted)
    return;
  auto* fitter = TVirtualFitter::GetFitter();
  if(fitter && fitter->GetObjectFit() == fSubtracted.get()) {
    TVirtualFitter::SetFitter(nullptr);
    delete fitter;
  }
}

// ============== PhotoPeakFitInput::Describe ==============
// Purpose: Record the effective background used by the fit.
// Inputs: Result and requested settings. Outputs: Reportable background snapshot and errors.
void PhotoPeakFitInput::Describe(PhotoPeakFitResult& result, const PhotoPeakFitConfig& config) const {
  result.backgroundMode = config.background == PhotoPeakBackgroundMode::Global ? "Global" :
    config.background == PhotoPeakBackgroundMode::Local ? "Local" : "None";
  result.backgroundSubtracted = background.histogram != nullptr;
  result.backgroundIterations = config.backgroundIterations;
  result.backgroundLow = background.low;
  result.backgroundHigh = background.high;
  result.message = background.error;
}

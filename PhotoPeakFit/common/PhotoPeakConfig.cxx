#include <PhotoPeakFit/PhotoPeakFitConfig.h>

#include <algorithm>

// ============== PhotoPeakFitConfig::Defaults ==============
// Purpose: Build defaults matching the PhotoPeak macro configuration.
// Inputs: None.
// Outputs: Normalized default parameter controls.
PhotoPeakFitConfig PhotoPeakFitConfig::Defaults() {
  PhotoPeakFitConfig config;
  return config;
}

// ============== PhotoPeakFitConfig::Normalize ==============
// Purpose: Enforce mode and relative-parameter invariants.
// Inputs: Number of configured peak seeds.
// Outputs: Self-consistent configuration.
void PhotoPeakFitConfig::Normalize(std::size_t peakCount) {
  if(peakCount < 2) {
    relativePosition = false;
    relativeFwhm = false;
  }
  auto& r = global[kPhotoPeakR];
  auto& beta = global[kPhotoPeakBeta];
  if(r.mode == PhotoPeakParameterMode::Fixed && r.value == 0.0) {
    beta.value = 0.0;
    beta.mode = PhotoPeakParameterMode::Fixed;
  }
  if(widthScale.lower > widthScale.upper)
    std::swap(widthScale.lower, widthScale.upper);
  if(mode == PhotoPeakFitMode::LowStat) {
    global[kPhotoPeakR] = {0.0, PhotoPeakParameterMode::Fixed, 0.0, 0.0};
    global[kPhotoPeakBeta] = {0.0, PhotoPeakParameterMode::Fixed, 0.0, 0.0};
    global[kPhotoPeakStep] = {0.0, PhotoPeakParameterMode::Fixed, 0.0, 0.0};
    global[kPhotoPeakC] = {0.0, PhotoPeakParameterMode::Fixed, 0.0, 0.0};
  }
}

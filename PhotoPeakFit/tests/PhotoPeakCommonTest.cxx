#include <PhotoPeakFit/PhotoPeakFitter.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include <TFile.h>
#include <TH1.h>
#include <TH1D.h>

namespace {

// ============== Near ==============
// Purpose: Compare a value with one reference using relative tolerance.
// Inputs: Actual value, reference value, and relative tolerance.
// Outputs: True when the values agree.
bool Near(double actual, double expected, double tolerance) {
  return std::fabs(actual - expected) <=
    tolerance * std::max(1.0, std::fabs(expected));
}

// ============== Fail ==============
// Purpose: Report one common-test failure.
// Inputs: Failure message.
// Outputs: Process failure code.
int Fail(const char* message) {
  std::fprintf(stderr, "PhotoPeakCommonTest: %s\n", message);
  return 1;
}

} // namespace

// ============== main ==============
// Purpose: Validate the ROOT-only single-photopeak numerical baseline.
// Inputs: None.
// Outputs: Process success or failure code.
int main() {
  TFile file(PHOTOPEAK_TEST_DATA, "READ");
  auto* hist = file.Get<TH1>("ParticleGates/Er/GammaEfficiency_Er");
  if(!hist)
    return Fail("test histogram was not found");

  const PhotoPeakFitResult result =
    PhotoPeakFitter::Fit(hist, 175.0, 210.0, 191.75, nullptr, false);
  if(result.status != 0)
    return Fail("fit status was not zero");
  if(result.model != "gaussian_linearBg_tail_quadBg")
    return Fail("auto mode selected the wrong candidate model");
  if(!Near(result.parameters[kPhotoPeakPosition], 192.037584, 5.0e-6))
    return Fail("centroid differs from the macro baseline");
  if(!Near(result.parameters[kPhotoPeakFwhm], 2.332954, 5.0e-6))
    return Fail("FWHM differs from the macro baseline");
  if(!Near(result.area, 8.617839e6, 5.0e-6))
    return Fail("area differs from the macro baseline");
  if(!Near(result.areaError, 37094.31696, 5.0e-6))
    return Fail("area uncertainty differs from the macro baseline");

  TH1D multi("multi_test", "multi_test", 700, 150.0, 220.0);
  for(int bin = 1; bin <= multi.GetNbinsX(); ++bin) {
    const double x = multi.GetBinCenter(bin);
    const double value = 500.0 + 2.0 * (x - 190.0) +
      30000.0 * std::exp(-std::pow((x - 184.0) / 1.25, 2.0)) +
      22000.0 * std::exp(-std::pow((x - 199.0) / 1.45, 2.0));
    multi.SetBinContent(bin, value);
    multi.SetBinError(bin, std::sqrt(std::max(value, 1.0)));
  }
  PhotoPeakFitRequest multiRequest;
  multiRequest.fitLow = 175.0;
  multiRequest.fitHigh = 207.0;
  multiRequest.config.relativeFwhm = false;
  PhotoPeakSeed first;
  first.centroid.value = 184.0;
  first.fwhm.value = 2.08;
  first.height.value = 30000.0;
  PhotoPeakSeed second;
  second.centroid.value = 199.0;
  second.fwhm.value = 2.41;
  second.height.value = 22000.0;
  multiRequest.peaks = {first, second};
  const auto multiResult = PhotoPeakFitter::Fit(&multi, multiRequest, nullptr, false);
  if(multiResult.status != 0 || multiResult.peaks.size() != 2)
    return Fail("multi-peak fit did not converge with two results");
  if(!Near(multiResult.peaks[0].centroid, 184.0, 2.0e-3) ||
     !Near(multiResult.peaks[1].centroid, 199.0, 2.0e-3))
    return Fail("multi-peak centroids differ from the synthetic baseline");
  return 0;
}

#include <PhotoPeakFit/PhotoPeakFitter.h>

#include <cmath>
#include <cstdio>
#include <string>

#include <TFile.h>
#include <TH1.h>

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
  return 0;
}

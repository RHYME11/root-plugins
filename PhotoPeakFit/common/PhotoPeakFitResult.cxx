#include <PhotoPeakFit/PhotoPeakFitResult.h>

#include <iomanip>
#include <sstream>

// ============== PhotoPeakFitResult::GlobalParameterReport ==============
// Purpose: Share fitted global values between the panel and terminal.
// Inputs: This fit snapshot. Outputs: English parameter values and uncertainties.
std::string PhotoPeakFitResult::GlobalParameterReport() const {
  const int indices[] = {kPhotoPeakR, kPhotoPeakBeta, kPhotoPeakStep,
    kPhotoPeakA, kPhotoPeakB, kPhotoPeakC};
  const char* names[] = {"R", "BETA", "STEP", "A", "B", "C"};
  std::ostringstream output;
  output << std::setprecision(10) << "Shared parameters:\n";
  for(int index = 0; index < 6; ++index) {
    output << "  " << names[index] << ": " << parameters[indices[index]]
      << " +/- " << errors[indices[index]] << "\n";
  }
  if(relativeFwhm)
    output << "  WSCALE: " << widthScale << " +/- " << widthScaleError << "\n";
  return output.str();
}

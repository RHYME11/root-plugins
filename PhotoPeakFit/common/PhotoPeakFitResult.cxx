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

// ============== PhotoPeakFitResult::BackgroundReport ==============
// Purpose: Explain the background treatment used for this fit snapshot.
// Inputs: This result. Outputs: Effective scope, settings and display scale.
std::string PhotoPeakFitResult::BackgroundReport() const {
  std::ostringstream output;
  output << std::setprecision(10) << "TSpectrum background: " << backgroundMode << "\n";
  if(backgroundSubtracted) {
    output << "  Iterations: " << backgroundIterations << "\n  Estimate range: ["
      << backgroundLow << ", " << backgroundHigh << "]\n"
      << "  Background subtracted for fitting; added back to displayed curves.\n"
      << "  A/B/C describe the remaining background.\n";
  }
  if(!message.empty())
    output << message << "\n";
  return output.str();
}

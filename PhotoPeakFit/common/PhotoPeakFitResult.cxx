#include <PhotoPeakFit/PhotoPeakFitResult.h>
#include <PhotoPeakFit/PhotoPeakFitRequest.h>

#include <iomanip>
#include <sstream>

namespace {

// ============== ConstraintTag ==============
// Purpose: Describe the actual constraints used by ROOT, including model restrictions.
// Inputs: Fitted parameter snapshot. Outputs: Macro-style fixed/limit marker.
std::string ConstraintTag(const PhotoPeakFitResult::Parameter& parameter) {
  std::ostringstream output;
  output << std::setprecision(10);
  if(parameter.fixed)
    output << " (fix " << parameter.value << ")";
  else if(parameter.lower != 0.0 || parameter.upper != 0.0)
    output << " (limit " << parameter.lower << " " << parameter.upper << ")";
  return output.str();
}

} // namespace

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

// ============== PhotoPeakFitResult::Report ==============
// Purpose: Share the macro-style fit report between the terminal and Results page.
// Inputs: Target name and fitted request snapshot. Outputs: Metadata, derived peaks and raw parameters.
std::string PhotoPeakFitResult::Report(const std::string& histogram, const PhotoPeakFitRequest& request) const {
  const bool multiple = request.peaks.size() > 1;
  const bool relativePosition = multiple && request.config.relativePosition;
  const char* mode = request.config.mode == PhotoPeakFitMode::Auto ? "auto" :
    request.config.mode == PhotoPeakFitMode::HighStat ? "highstat" : "lowstat";
  std::ostringstream output;
  output << std::setprecision(10) << (multiple ? "multipeakfit" : "photopeakfit")
    << " result for " << histogram << "\nFit range: [" << request.fitLow << ", " << request.fitHigh << "]"
    << (multiple ? ", initial peaks:" : ", initial peak position:");
  for(const auto& seed : request.peaks)
    output << " " << seed.centroid.value;
  output << "\nRequested mode: " << mode << "\nROOT fit option: " << request.config.rootOptions << "\n"
    << BackgroundReport()
    << "Relative position fixed: " << (relativePosition ? "true" : "false")
    << "\nRelative FWHM fixed: " << (relativeFwhm ? "true" : "false")
    << "\nFitting function: " << model << "\nFit bins = " << fitBins << ", free parameters = " << freeParameters
    << "\nFit status: " << status << "\n";
  if(model.empty())
    return output.str();
  output << "chi2 = " << chi2 << ", ndf = " << ndf << ", reduced chisq = ";
  if(ndf > 0)
    output << reducedChi2;
  else
    output << "n/a";
  output << "\n\nPeaks:\n";
  auto fittedPeaks = peaks;
  if(fittedPeaks.empty() && !multiple) {
    Peak peak;
    peak.centroid = parameters[kPhotoPeakPosition];
    peak.centroidError = errors[kPhotoPeakPosition];
    peak.height = parameters[kPhotoPeakHeight];
    peak.heightError = errors[kPhotoPeakHeight];
    peak.fwhm = parameters[kPhotoPeakFwhm];
    peak.fwhmError = errors[kPhotoPeakFwhm];
    peak.area = area;
    peak.areaError = areaError;
    fittedPeaks.push_back(peak);
  }
  const std::size_t reference = multiple ? 7 : kPhotoPeakPosition;
  const bool positionFixed = reference < fitParameters.size() && fitParameters[reference].fixed;
  for(std::size_t index = 0; index < fittedPeaks.size(); ++index) {
    const auto& peak = fittedPeaks[index];
    const auto id = index < request.peaks.size() && request.peaks[index].id ? request.peaks[index].id : index + 1;
    const std::size_t positionIndex = multiple ? 7 + 3 * index : kPhotoPeakPosition;
    const std::size_t widthIndex = multiple ? positionIndex + 1 : kPhotoPeakFwhm;
    const std::size_t heightIndex = multiple ? positionIndex + 2 : kPhotoPeakHeight;
    auto tag = [&](std::size_t parameter) {
      return parameter < fitParameters.size() ? ConstraintTag(fitParameters[parameter]) : std::string{};
    };
    const bool fixed = relativePosition ? positionFixed :
      positionIndex < fitParameters.size() && fitParameters[positionIndex].fixed;
    const bool widthFixed = relativeFwhm ? fitParameters.size() > 6 && fitParameters[6].fixed :
      widthIndex < fitParameters.size() && fitParameters[widthIndex].fixed;
    const char* positionSource = relativePosition && index > 0 ?
      (positionFixed ? "derived from fixed P0" : "derived from P0") : fixed ? "fixed" : "fit";
    const char* widthSource = relativeFwhm ?
      (widthFixed ? "derived from fixed WSCALE" : "derived from WSCALE") : widthFixed ? "fixed" : "fit";
    output << "  Peak " << id << ":\n"
      << "    Position = " << peak.centroid << " +/- " << peak.centroidError
      << " (" << positionSource << ")"
      << (relativePosition && index > 0 ? std::string{} : tag(positionIndex))
      << "\n    Height   = " << peak.height << " +/- " << peak.heightError << tag(heightIndex)
      << "\n    FWHM     = " << peak.fwhm << " +/- " << peak.fwhmError
      << " (" << widthSource << ")"
      << tag(relativeFwhm ? 6 : widthIndex)
      << "\n    Area     = " << peak.area << " +/- " << peak.areaError << "\n";
  }
  output << "\nTotal photopeak area = " << area << " +/- " << areaError << "\n\nFit parameters:\n";
  auto printParameter = [&](std::size_t index) {
    if(index >= fitParameters.size())
      return;
    const auto& parameter = fitParameters[index];
    output << "  " << std::left << std::setw(8) << parameter.name << " = "
      << parameter.value << " +/- " << parameter.error << ConstraintTag(parameter);
    if((index == kPhotoPeakR || index == kPhotoPeakBeta) && model.find("tail") == std::string::npos)
      output << " (inactive in selected model)";
    else if(index == kPhotoPeakStep && model.find("step") == std::string::npos)
      output << " (inactive in selected model)";
    else if(index == kPhotoPeakC && model.find("quadBg") == std::string::npos)
      output << " (inactive in selected model)";
    else if(multiple && index == 6 && !relativeFwhm)
      output << " (inactive: individual FWHM)";
    else if(multiple && index >= 7 && (index - 7) % 3 == 0 && index > 7 && relativePosition)
      output << " (reference offset; position derived from P0)";
    else if(multiple && index >= 7 && (index - 7) % 3 == 1 && relativeFwhm)
      output << " (reference width for WSCALE)";
    output << "\n";
  };
  if(multiple) {
    for(std::size_t index = 0; index < fitParameters.size(); ++index)
      printParameter(index);
  } else {
    for(int index : {kPhotoPeakPosition, kPhotoPeakHeight, kPhotoPeakFwhm,
      kPhotoPeakR, kPhotoPeakBeta, kPhotoPeakStep, kPhotoPeakA, kPhotoPeakB, kPhotoPeakC})
      printParameter(index);
  }
  return output.str();
}

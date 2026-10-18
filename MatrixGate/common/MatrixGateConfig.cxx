#include <MatrixGate/MatrixGateConfig.h>

namespace MatrixGate {

// ============== ToString ==============
// Purpose: Name the gate axis in reports and ROOT commands.
// Inputs: Axis value. Outputs: Static English label.
const char* ToString(Axis axis) {
  switch(axis) {
    case Axis::X: return "X";
    case Axis::Y: return "Y";
  }
  return "Invalid";
}

// ============== ToString ==============
// Purpose: Name the multiple-gate operation without conflating Sum and Union.
// Inputs: Combination value. Outputs: Static English label.
const char* ToString(Combination combination) {
  switch(combination) {
    case Combination::Separate: return "Separate";
    case Combination::Sum: return "Sum";
    case Combination::Union: return "Union";
  }
  return "Invalid";
}

// ============== ToString ==============
// Purpose: Name the mathematical subtraction model.
// Inputs: Model value. Outputs: Static English label.
const char* ToString(SubtractionModel model) {
  switch(model) {
    case SubtractionModel::None: return "None";
    case SubtractionModel::Template: return "jROOT/SLICE template subtraction";
    case SubtractionModel::RadWareStandard: return "RadWare standard 2D background";
  }
  return "Invalid";
}

// ============== ToString ==============
// Purpose: Name the source of the background template.
// Inputs: Template value. Outputs: Static English label.
const char* ToString(BackgroundTemplate selection) {
  switch(selection) {
    case BackgroundTemplate::None: return "None";
    case BackgroundTemplate::Full: return "Full";
    case BackgroundTemplate::Compton: return "Compton";
    case BackgroundTemplate::Anti: return "Anti";
    case BackgroundTemplate::Manual: return "Manual";
    case BackgroundTemplate::ManualRelative: return "Manual Relative";
  }
  return "Invalid";
}

// ============== ToString ==============
// Purpose: Name the background-fraction estimator.
// Inputs: Estimator value. Outputs: Static English label.
const char* ToString(FractionMethod method) {
  switch(method) {
    case FractionMethod::LinearFixed: return "Linear Fixed";
    case FractionMethod::PolynomialFlex: return "Polynomial Flex";
    case FractionMethod::Manual: return "Manual fraction";
    case FractionMethod::TSpectrum: return "TSpectrum";
    case FractionMethod::Width: return "Window width ratio";
  }
  return "Invalid";
}

// ============== ToString ==============
// Purpose: Name the statistical approximation and preset.
// Inputs: Error policy. Outputs: Static English label.
const char* ToString(ErrorPolicy policy) {
  switch(policy) {
    case ErrorPolicy::FixedCoefficients: return "Fixed coefficients with shared-bin covariance";
    case ErrorPolicy::JRootCompatible: return "jROOT compatible: neglect signal/template covariance";
    case ErrorPolicy::RadWareCompatible: return "RadWare compatible: fixed background, variance floor 1";
  }
  return "Invalid";
}

} // namespace MatrixGate

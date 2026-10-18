#ifndef MATRIXGATE_CONFIG_H
#define MATRIXGATE_CONFIG_H

#include <cstdint>
#include <string>
#include <vector>

namespace MatrixGate {

enum class Axis { X, Y };
enum class Combination { Separate, Sum, Union };
enum class SubtractionModel { None, Template, RadWareStandard };
enum class BackgroundTemplate { None, Full, Compton, Anti, Manual, ManualRelative };
enum class FractionMethod { LinearFixed, PolynomialFlex, Manual, TSpectrum, Width };
enum class ErrorPolicy { FixedCoefficients, JRootCompatible, RadWareCompatible };
enum class FractionIntegral { Counts, WidthWeighted };
enum class ManualOverlap { Reference, ExcludeSignal, Allow };

struct Gate {
  std::uint64_t id = 0;
  std::string name;
  double low = 0.0;
  double high = 0.0;
  bool enabled = true;
};

struct FlowPolicy {
  // Gate-axis flow bins join only gates touching the corresponding axis endpoint.
  bool gateUnderflow = false;
  bool gateOverflow = false;
  // Result-axis flow bins also participate in the total projection's sums.
  bool resultUnderflow = false;
  bool resultOverflow = false;
};

struct BackgroundConfig {
  SubtractionModel model = SubtractionModel::None;
  BackgroundTemplate selection = BackgroundTemplate::None;
  FractionMethod fractionMethod = FractionMethod::TSpectrum;
  std::vector<Gate> gates;
  double fraction = 0.0;
  // Negative one selects the error-policy preset; nonnegative values override it.
  double relativeUncertainty = -1.0;
  FractionIntegral fractionIntegral = FractionIntegral::Counts;
  ManualOverlap manualOverlap = ManualOverlap::Reference;
  int comptonGapBins = 1;
  int spectrumIterations = 25;
  std::string spectrumOptions;
  bool useFitRange = false;
  double fitLow = 0.0;
  double fitHigh = 0.0;
  bool useNormalizationRange = false;
  double normalizationLow = 0.0;
  double normalizationHigh = 0.0;
  // Normal-bin background counts on the gate axis, with exactly N entries.
  std::vector<double> projectionBackground;
  bool symmetricGammaGamma = false;
  double symmetryTolerance = 1.0e-10;
};

struct Config {
  Axis gateAxis = Axis::X;
  Combination combination = Combination::Separate;
  std::vector<Gate> gates;
  FlowPolicy flow;
  BackgroundConfig background;
  ErrorPolicy errors = ErrorPolicy::FixedCoefficients;
};

// ============== ToString ==============
// Purpose: Name the gate axis in reports and ROOT commands.
// Inputs: Axis value. Outputs: Static English label.
const char* ToString(Axis axis);

// ============== ToString ==============
// Purpose: Name the multiple-gate operation without conflating Sum and Union.
// Inputs: Combination value. Outputs: Static English label.
const char* ToString(Combination combination);

// ============== ToString ==============
// Purpose: Name the mathematical subtraction model.
// Inputs: Model value. Outputs: Static English label.
const char* ToString(SubtractionModel model);

// ============== ToString ==============
// Purpose: Name the source of the background template.
// Inputs: Template value. Outputs: Static English label.
const char* ToString(BackgroundTemplate selection);

// ============== ToString ==============
// Purpose: Name the background-fraction estimator.
// Inputs: Estimator value. Outputs: Static English label.
const char* ToString(FractionMethod method);

// ============== ToString ==============
// Purpose: Name the statistical approximation and preset.
// Inputs: Error policy. Outputs: Static English label.
const char* ToString(ErrorPolicy policy);

} // namespace MatrixGate

#endif

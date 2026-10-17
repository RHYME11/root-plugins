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
  // Reserved modes are rejected until their calculation stage is implemented.
  SubtractionModel model = SubtractionModel::None;
  BackgroundTemplate selection = BackgroundTemplate::None;
  FractionMethod fractionMethod = FractionMethod::TSpectrum;
  std::vector<Gate> gates;
  double fraction = 0.0;
  double relativeUncertainty = 0.0;
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

} // namespace MatrixGate

#endif

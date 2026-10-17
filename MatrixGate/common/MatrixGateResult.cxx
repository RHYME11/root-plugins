#include <MatrixGate/MatrixGateResult.h>

#include <iomanip>
#include <limits>
#include <sstream>

namespace MatrixGate {

// ============== Result::Report ==============
// Purpose: Describe actual source-bin selections and their statistical convention.
// Inputs: This result. Outputs: English report suitable for ROOT or a future GUI.
std::string Result::Report() const {
  std::ostringstream output;
  output << std::setprecision(std::numeric_limits<double>::max_digits10);
  output << "MatrixGate: " << (success ? "ready" : "failed") << '\n'
         << "Source: " << sourceName << '\n'
         << "Gate axis: " << ToString(config.gateAxis) << " -> Result axis: "
         << ToString(config.gateAxis == Axis::X ? Axis::Y : Axis::X) << '\n'
         << "Combination: " << ToString(config.combination) << '\n'
         << "Status: " << message << '\n';
  if(!success)
    return output.str();
  output << "Background: None\n"
         << "Gate flow bins: underflow=" << config.flow.gateUnderflow
         << ", overflow=" << config.flow.gateOverflow << '\n'
         << "Result flow bins: underflow=" << config.flow.resultUnderflow
         << ", overflow=" << config.flow.resultOverflow << '\n'
         << "Statistics: fixed bin weights; independent source bins; V = sum(w^2 Vbin).\n"
         << "Separate spectra may share source bins and are not necessarily independent.\n";
  for(std::size_t index = 0; index < projections.size(); ++index) {
    const auto& projection = projections[index];
    output << "Projection " << index + 1 << ": " << projection.raw->GetName()
           << ", weighted normal-bin width=" << projection.width << '\n';
    for(const auto& gate : projection.gates) {
      output << "  Gate " << gate.requested.id << " (" << gate.requested.name << "): requested ["
             << gate.requested.low << ", " << gate.requested.high << "), bins "
             << gate.first << ".." << gate.last << ", actual [" << gate.low << ", "
             << gate.high << "), width=" << gate.width << ", underflow=" << gate.underflow
             << ", overflow=" << gate.overflow << '\n';
    }
  }
  return output.str();
}

} // namespace MatrixGate

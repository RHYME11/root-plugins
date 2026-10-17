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

} // namespace MatrixGate

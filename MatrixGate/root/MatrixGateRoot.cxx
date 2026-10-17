#include <MatrixGate/MatrixGateRoot.h>

#include "MatrixGateProjection.h"

namespace MatrixGate {

// ============== Compute ==============
// Purpose: Expose the projection engine to compiled code and native ROOT.
// Inputs: Matrix and configuration. Outputs: Owned result or failure diagnostic.
Result Compute(const TH2& matrix, const Config& config) {
  return Detail::ComputeProjection(matrix, config);
}

} // namespace MatrixGate

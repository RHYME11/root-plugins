#include <MatrixGate/MatrixGateRoot.h>

#include "MatrixGateProjection.h"
#include <MatrixGate/detail/MatrixGateRootController.h>

namespace MatrixGate {

// ============== Compute ==============
// Purpose: Expose the projection engine to compiled code and native ROOT.
// Inputs: Matrix and configuration. Outputs: Owned result or failure diagnostic.
Result Compute(const TH2& matrix, const Config& config) {
  return Detail::ComputeProjection(matrix, config);
}

// ============== Start ==============
// Purpose: Open a native analysis window. Inputs: Matrix and settings. Outputs: API status.
bool Start(TH2* matrix, const Config& config) {
  return Detail::RootController::Instance().Start(matrix, config);
}

// ============== Update ==============
// Purpose: Recompute an active session. Inputs: Matrix. Outputs: API status.
bool Update(TH2* matrix) { return Detail::RootController::Instance().Update(matrix); }

// ============== Stop ==============
// Purpose: Close an active session. Inputs: Matrix identity. Outputs: API status.
bool Stop(TH2* matrix) { return Detail::RootController::Instance().Stop(matrix); }

// ============== StopAll ==============
// Purpose: Close every session. Inputs: None. Outputs: Released interactive state.
void StopAll() { Detail::RootController::Instance().StopAll(); }

// ============== LastError ==============
// Purpose: Read the last interactive diagnostic. Inputs: None. Outputs: Borrowed message.
const char* LastError() { return Detail::RootController::Instance().LastError(); }

} // namespace MatrixGate

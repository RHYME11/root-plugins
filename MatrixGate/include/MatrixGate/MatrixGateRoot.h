#ifndef MATRIXGATE_ROOT_H
#define MATRIXGATE_ROOT_H

#include <MatrixGate/MatrixGateResult.h>

class TH2;

namespace MatrixGate {

// ============== Compute ==============
// Purpose: Project a rectangular matrix without changing its data or view.
// Inputs: Matrix and configuration; coordinate gates use [low, high).
// Outputs: Owned detached spectra, or success=false with a diagnostic and no spectra.
Result Compute(const TH2& matrix, const Config& config = Config{});

// ============== Start ==============
// Purpose: Open or raise an independent native ROOT analysis window for a matrix.
// Inputs: Borrowed live TH2 and optional initial configuration.
// Outputs: Success, or false with a diagnostic from LastError().
bool Start(TH2* matrix, const Config& config = Config{});

// ============== Update ==============
// Purpose: Commit pending edits and recompute after external matrix changes.
// Inputs: Matrix owning an active window. Outputs: Success or a diagnostic.
bool Update(TH2* matrix);

// ============== Stop ==============
// Purpose: Close one analysis window and release its event connections.
// Inputs: Matrix identity. Outputs: True when an active session was closed.
bool Stop(TH2* matrix);

// ============== StopAll ==============
// Purpose: Close all MatrixGate analysis windows.
// Inputs: None. Outputs: Released sessions and deferred native window cleanup.
void StopAll();

// ============== LastError ==============
// Purpose: Read the latest interactive API diagnostic.
// Inputs: None. Outputs: Message valid until the next interactive API call.
const char* LastError();

} // namespace MatrixGate

#endif

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

} // namespace MatrixGate

#endif

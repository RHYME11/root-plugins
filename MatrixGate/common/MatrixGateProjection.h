#ifndef MATRIXGATE_PROJECTION_H
#define MATRIXGATE_PROJECTION_H

#include <MatrixGate/MatrixGateResult.h>

class TH2;

namespace MatrixGate::Detail {

// ============== ComputeProjection ==============
// Purpose: Validate gates and calculate total and gated spectra.
// Inputs: Read-only rectangular matrix and configuration.
// Outputs: Detached owned spectra or an atomic failure result.
Result ComputeProjection(const TH2& matrix, const Config& config);

} // namespace MatrixGate::Detail

#endif

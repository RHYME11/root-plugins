#ifndef MATRIXGATE_BACKGROUND_H
#define MATRIXGATE_BACKGROUND_H

#include <MatrixGate/MatrixGateResult.h>

class TH2;

namespace MatrixGate::Detail {

// ============== ValidateBackgroundConfig ==============
// Purpose: Reject unsupported or inconsistent model and uncertainty settings.
// Inputs: Committed configuration. Outputs: Exception on invalid settings.
void ValidateBackgroundConfig(const Config& config);

// ============== ApplyBackground ==============
// Purpose: Form templates, calculate normalization, and propagate conditional errors.
// Inputs: Read-only matrix and its owned raw projections. Outputs: Background and net spectra.
void ApplyBackground(const TH2& matrix, Result& result);

} // namespace MatrixGate::Detail

#endif

#ifndef MATRIXGATE_PROJECTION_H
#define MATRIXGATE_PROJECTION_H

#include <MatrixGate/MatrixGateResult.h>

class TH2;
class TAxis;

namespace MatrixGate::Detail {

// ============== ReadEdges ==============
// Purpose: Read validated physical bin boundaries.
// Inputs: Source axis. Outputs: N+1 strictly increasing finite edges.
std::vector<double> ReadEdges(const TAxis& axis);

// ============== ResolveGate ==============
// Purpose: Resolve a half-open interval without rounded coordinate-to-bin arithmetic.
// Inputs: Gate, validated edges, and flow policy. Outputs: Actual selected bins.
GateBins ResolveGate(const Gate& gate, const std::vector<double>& edges, const FlowPolicy& flow);

// ============== MakeSpectrum ==============
// Purpose: Create a detached spectrum with the original axis grid and labels.
// Inputs: Name, title, axis, edges, and count-axis label. Outputs: Owned TH1D.
std::unique_ptr<TH1D> MakeSpectrum(const std::string& name, const std::string& title,
  const TAxis& axis, const std::vector<double>& edges, const char* countTitle);

// ============== ComputeProjection ==============
// Purpose: Validate gates and calculate total and gated spectra.
// Inputs: Read-only rectangular matrix and configuration.
// Outputs: Detached owned spectra or an atomic failure result.
Result ComputeProjection(const TH2& matrix, const Config& config);

} // namespace MatrixGate::Detail

#endif

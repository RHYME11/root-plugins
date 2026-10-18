#include <MatrixGate/MatrixGateResult.h>

#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace MatrixGate {

// ============== Projection::Covariance ==============
// Purpose: Read conditional statistical and common-scale covariance without dense matrix storage.
// Inputs: ROOT bin indices. Outputs: Covariance or out_of_range for an invalid index.
double Projection::Covariance(int first, int second) const {
  if(first < 0 || second < 0 || static_cast<std::size_t>(first) >= statisticalVariance.size() ||
     static_cast<std::size_t>(second) >= statisticalVariance.size())
    throw std::out_of_range("Covariance bin index is outside the stored spectrum.");
  return systematicShift[first] * systematicShift[second] +
    (first == second ? statisticalVariance[first] + varianceFloor[first] : 0.0);
}

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
  output << "Model: " << ToString(config.background.model) << '\n'
         << "Template: " << ToString(config.background.selection) << '\n'
         << "Fraction method: " << ToString(config.background.fractionMethod) << '\n'
         << "Fraction integral: " << (config.background.fractionIntegral == FractionIntegral::Counts ?
              "Counts" : "Width-weighted ordinates (jROOT convention)") << '\n'
         << "Background curve: " << (backgroundSource.empty() ? "Not used" : backgroundSource) << '\n'
         << "Error policy: " << ToString(config.errors) << '\n'
         << "Gate flow bins: underflow=" << config.flow.gateUnderflow
         << ", overflow=" << config.flow.gateOverflow << '\n'
         << "Result flow bins: underflow=" << config.flow.resultUnderflow
         << ", overflow=" << config.flow.resultOverflow << '\n'
         << "Statistics: fixed coefficients and independent source bins.\n"
         << "Estimated fractions, fitted baselines, normalization ratios, and RadWare backgrounds are held fixed.\n"
         << "One common scale nuisance gives bin covariance shift_i * shift_j; TH1 stores diagonal errors.\n"
         << "Separate spectra may share source bins and are not necessarily independent.\n";
  if(backgroundAboveTotalBins > 0)
    output << "Background estimate exceeds observed total in " << backgroundAboveTotalBins
           << " bins; maximum excess=" << maximumBackgroundExcess << ". The curve is retained without clipping.\n";
  for(std::size_t index = 0; index < projections.size(); ++index) {
    const auto& projection = projections[index];
    output << "Projection " << index + 1 << ": " << projection.raw->GetName()
           << ", weighted normal-bin width=" << projection.width << '\n'
           << "  Normalization bins: " << projection.normalizationFirst << ".." << projection.normalizationLast
           << "; relative scale uncertainty=" << projection.relativeUncertainty << '\n';
    for(const auto& gate : projection.gates) {
      output << "  Gate " << gate.requested.id << " (" << gate.requested.name << "): requested ["
             << gate.requested.low << ", " << gate.requested.high << "), bins "
             << gate.first << ".." << gate.last << ", actual [" << gate.low << ", "
             << gate.high << "), width=" << gate.width << ", underflow=" << gate.underflow
             << ", overflow=" << gate.overflow << '\n';
    }
    for(const auto& component : projection.components) {
      output << "  Subtraction component gates:";
      for(const auto id : component.gateIds) output << ' ' << id;
      output << "; f=";
      if(component.fractionDefined) output << component.fraction;
      else output << "undefined (nonpositive signal integral)";
      output << "; alpha=" << component.alpha
             << "; beta=" << component.beta << "; signal integral=" << component.rawIntegral
             << "; template integral=" << component.templateIntegral
             << "; template width=" << component.backgroundWidth << '\n';
      if(component.fractionClamped)
        output << "    Estimated fraction clamped from " << component.unclampedFraction << " to [0,1].\n";
      if(component.fitStatus >= 0) {
        output << "    Local fit: status=" << component.fitStatus << "; range=[" << component.fitLow << ", "
               << component.fitHigh << "); centroid=" << component.fittedCentroid
               << "; sigma=" << component.fittedSigma << "; chi2/NDF=" << component.fitChi2 << '/'
               << component.fitNdf << "; polynomial origin=" << component.fitOrigin << '\n'
               << "    Parameters (height, mean, sigma, c0, c1, c2):";
        for(const double value : component.fitParameters) output << ' ' << value;
        output << '\n';
      }
      for(const auto& gate : component.backgroundGates)
        output << "    Background gate " << gate.requested.id << ": bins " << gate.first << ".." << gate.last
               << "; actual=[" << gate.low << ", " << gate.high << ")\n";
    }
  }
  return output.str();
}

} // namespace MatrixGate

#include "MatrixGateProjection.h"
#include "MatrixGateBackground.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

#include <TAxis.h>
#include <TDirectory.h>
#include <TH2.h>

namespace MatrixGate::Detail {

static std::atomic<std::uint64_t> gCalculationId{0};

// ============== ReadEdges ==============
// Purpose: Read physical bin edges without relying on rounded coordinate-to-bin arithmetic.
// Inputs: Source axis. Outputs: Validated edges or an exception.
std::vector<double> ReadEdges(const TAxis& axis) {
  if(axis.GetNbins() < 1)
    throw std::invalid_argument("Matrix axes require at least one bin.");
  std::vector<double> edges;
  edges.reserve(axis.GetNbins() + 1);
  edges.push_back(axis.GetBinLowEdge(1));
  if(!std::isfinite(edges.back()))
    throw std::invalid_argument("Matrix bin edges must be finite.");
  for(int bin = 1; bin <= axis.GetNbins(); ++bin) {
    const double edge = axis.GetBinUpEdge(bin);
    if(!std::isfinite(edge) || edge <= edges.back())
      throw std::invalid_argument("Matrix bin edges must be finite and strictly increasing.");
    edges.push_back(edge);
  }
  if(!std::isfinite(edges.back() - edges.front()))
    throw std::invalid_argument("Matrix axis width must be finite.");
  return edges;
}

// ============== ValidateConfig ==============
// Purpose: Reject unsupported or ambiguous configuration before calculation.
// Inputs: Committed configuration. Outputs: Exception for invalid requests.
void ValidateConfig(const MatrixGate::Config& config) {
  using namespace MatrixGate;
  if(config.gateAxis != Axis::X && config.gateAxis != Axis::Y)
    throw std::invalid_argument("Gate axis must be X or Y.");
  if(config.combination != Combination::Separate && config.combination != Combination::Sum &&
     config.combination != Combination::Union)
    throw std::invalid_argument("Gate combination must be Separate, Sum, or Union.");
  ValidateBackgroundConfig(config);
  std::set<std::uint64_t> ids;
  for(const auto& gate : config.gates) {
    if(!gate.id || !ids.insert(gate.id).second)
      throw std::invalid_argument("Every gate requires a unique, nonzero ID, including disabled gates.");
  }
}

// ============== ResolveGate ==============
// Purpose: Expand a half-open coordinate interval to complete source bins.
// Inputs: Enabled gate, actual edges, and flow policy. Outputs: Actual bin selection.
MatrixGate::GateBins ResolveGate(const MatrixGate::Gate& gate, const std::vector<double>& edges,
  const MatrixGate::FlowPolicy& flow) {
  const double minimum = edges.front();
  const double maximum = edges.back();
  if(!std::isfinite(gate.low) || !std::isfinite(gate.high) ||
     gate.low >= gate.high || gate.low < minimum || gate.high > maximum)
    throw std::invalid_argument("Gate " + std::to_string(gate.id) +
      ": require axis minimum <= Low < High <= axis maximum.");
  MatrixGate::GateBins selection;
  selection.requested = gate;
  selection.first = static_cast<int>(std::upper_bound(edges.begin(), edges.end(), gate.low) - edges.begin());
  selection.last = static_cast<int>(std::lower_bound(edges.begin(), edges.end(), gate.high) - edges.begin());
  selection.low = edges[selection.first - 1];
  selection.high = edges[selection.last];
  selection.width = selection.high - selection.low;
  selection.underflow = flow.gateUnderflow && gate.low == minimum;
  selection.overflow = flow.gateOverflow && gate.high == maximum;
  return selection;
}

// ============== MakeSpectrum ==============
// Purpose: Create an owned spectrum with source bin edges, labels, and units.
// Inputs: Unique name, display title, source axis and edges, and count-axis title.
// Outputs: Empty detached TH1D with allocated variances.
std::unique_ptr<TH1D> MakeSpectrum(const std::string& name, const std::string& title,
  const TAxis& axis, const std::vector<double>& edges, const char* countTitle) {
  TDirectory::TContext context(nullptr);
  auto spectrum = std::make_unique<TH1D>(name.c_str(), title.c_str(), axis.GetNbins(), edges.data());
  spectrum->SetDirectory(nullptr);
  if(!spectrum->GetSumw2N())
    spectrum->Sumw2();
  spectrum->GetXaxis()->SetTitle(axis.GetTitle());
  spectrum->GetYaxis()->SetTitle(countTitle && *countTitle ? countTitle : "Counts");
  if(axis.GetLabels()) {
    for(int bin = 1; bin <= axis.GetNbins(); ++bin)
      spectrum->GetXaxis()->SetBinLabel(bin, axis.GetBinLabel(bin));
  }
  return spectrum;
}

// ============== AddGate ==============
// Purpose: Accumulate source-bin multiplicities without losing overlap identity.
// Inputs: Projection, actual gate bins, and combination mode. Outputs: Updated weights.
void AddGate(MatrixGate::Projection& projection, const MatrixGate::GateBins& gate,
  MatrixGate::Combination combination) {
  const int first = gate.underflow ? 0 : gate.first;
  const int last = gate.overflow ? static_cast<int>(projection.weights.size()) - 1 : gate.last;
  for(int bin = first; bin <= last; ++bin) {
    if(combination == MatrixGate::Combination::Union)
      projection.weights[bin] = 1.0;
    else
      projection.weights[bin] += 1.0;
  }
  projection.gates.push_back(gate);
}

// ============== ComputeUnchecked ==============
// Purpose: Produce validated total and gated spectra from source-bin weights.
// Inputs: Matrix and configuration. Outputs: Complete result or an exception.
MatrixGate::Result ComputeUnchecked(const TH2& matrix, const MatrixGate::Config& config) {
  using namespace MatrixGate;
  ValidateConfig(config);
  if(matrix.GetDimension() != 2 || matrix.InheritsFrom("TH2Poly"))
    throw std::invalid_argument("MatrixGate requires a rectangular TH2.");
  if(matrix.GetBufferLength() > 0)
    throw std::invalid_argument("Flush the matrix fill buffer explicitly before computing projections.");
  const auto xEdges = ReadEdges(*matrix.GetXaxis());
  const auto yEdges = ReadEdges(*matrix.GetYaxis());
  const bool gateX = config.gateAxis == Axis::X;
  const TAxis& gateAxis = *(gateX ? matrix.GetXaxis() : matrix.GetYaxis());
  const TAxis& resultAxis = *(gateX ? matrix.GetYaxis() : matrix.GetXaxis());
  const auto& gateEdges = gateX ? xEdges : yEdges;
  const auto& resultEdges = gateX ? yEdges : xEdges;
  const int gateBins = gateAxis.GetNbins();
  const int resultBins = resultAxis.GetNbins();
  const int firstGate = config.flow.gateUnderflow ? 0 : 1;
  const int lastGate = config.flow.gateOverflow ? gateBins + 1 : gateBins;
  const int firstResult = config.flow.resultUnderflow ? 0 : 1;
  const int lastResult = config.flow.resultOverflow ? resultBins + 1 : resultBins;
  const std::string prefix = "MatrixGate_" + std::to_string(++gCalculationId);
  Result output;
  output.config = config;
  output.sourceName = matrix.GetName();
  output.sourceTitle = matrix.GetTitle();
  output.total = MakeSpectrum(prefix + "_total", "Total projection", gateAxis, gateEdges,
    matrix.GetZaxis()->GetTitle());
  for(const auto& gate : config.gates) {
    if(!gate.enabled)
      continue;
    const auto selection = ResolveGate(gate, gateEdges, config.flow);
    if(config.combination == Combination::Separate || output.projections.empty()) {
      Projection projection;
      projection.weights.resize(gateBins + 2, 0.0);
      output.projections.push_back(std::move(projection));
    }
    AddGate(output.projections.back(), selection, config.combination);
  }
  std::size_t index = 0;
  for(auto& projection : output.projections) {
    projection.raw = MakeSpectrum(prefix + "_raw_" + std::to_string(++index),
      "Raw gated projection", resultAxis, resultEdges, matrix.GetZaxis()->GetTitle());
    for(int bin = 1; bin <= gateBins; ++bin)
      projection.width += projection.weights[bin] * (gateEdges[bin] - gateEdges[bin - 1]);
    if(!std::isfinite(projection.width))
      throw std::overflow_error("Weighted gate width overflowed.");
  }
  std::vector<double> totalVariance(gateBins + 2, 0.0);
  std::vector<std::vector<double>> rawVariance(output.projections.size(),
    std::vector<double>(resultBins + 2, 0.0));
  for(int gateBin = firstGate; gateBin <= lastGate; ++gateBin) {
    double total = 0.0;
    for(int resultBin = firstResult; resultBin <= lastResult; ++resultBin) {
      const int bin = gateX ? matrix.GetBin(gateBin, resultBin) : matrix.GetBin(resultBin, gateBin);
      const double content = matrix.GetBinContent(bin);
      const double error = matrix.GetBinError(bin);
      const double variance = error * error;
      if(!std::isfinite(content) || !std::isfinite(error) || !std::isfinite(variance))
        throw std::invalid_argument("Selected matrix bins require finite contents and variances.");
      total += content;
      totalVariance[gateBin] += variance;
      for(std::size_t item = 0; item < output.projections.size(); ++item) {
        auto& projection = output.projections[item];
        const double weight = projection.weights[gateBin];
        if(weight == 0.0)
          continue;
        const double value = projection.raw->GetBinContent(resultBin) + weight * content;
        projection.raw->SetBinContent(resultBin, value);
        rawVariance[item][resultBin] += weight * weight * variance;
        if(!std::isfinite(value) || !std::isfinite(rawVariance[item][resultBin]))
          throw std::overflow_error("Gated projection contents or variances overflowed.");
      }
    }
    if(!std::isfinite(total) || !std::isfinite(totalVariance[gateBin]))
      throw std::overflow_error("Total projection contents or variances overflowed.");
    output.total->SetBinContent(gateBin, total);
    output.total->SetBinError(gateBin, std::sqrt(totalVariance[gateBin]));
  }
  output.total->ResetStats();
  for(std::size_t item = 0; item < output.projections.size(); ++item) {
    auto& raw = *output.projections[item].raw;
    for(int bin = firstResult; bin <= lastResult; ++bin)
      raw.SetBinError(bin, std::sqrt(rawVariance[item][bin]));
    raw.ResetStats();
  }
  ApplyBackground(matrix, output);
  output.success = true;
  output.message = output.projections.empty() ? "Total projection ready; no enabled signal gates." :
    "Total, raw gated, subtracted-background, and net spectra ready.";
  return output;
}

// ============== ComputeProjection ==============
// Purpose: Return an atomic failure without exposing partial spectra.
// Inputs: Matrix and configuration. Outputs: Complete result or diagnostic.
Result ComputeProjection(const TH2& matrix, const Config& config) {
  try {
    return ComputeUnchecked(matrix, config);
  } catch(const std::exception& error) {
    Result result;
    result.config = config;
    result.sourceName = matrix.GetName();
    result.sourceTitle = matrix.GetTitle();
    result.message = error.what();
    return result;
  }
}

} // namespace MatrixGate::Detail

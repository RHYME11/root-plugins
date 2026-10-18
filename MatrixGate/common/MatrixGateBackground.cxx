#include "MatrixGateBackground.h"
#include "MatrixGateProjection.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

#include <Fit/Fitter.h>
#include <Math/Functor.h>
#include <TH2.h>
#include <TSpectrum.h>

namespace MatrixGate::Detail {
namespace {

// ============== Require ==============
// Purpose: Report an invalid mathematical request without returning partial spectra.
// Inputs: Condition and diagnostic. Outputs: Exception when the condition fails.
void Require(bool condition, const std::string& message) {
  if(!condition)
    throw std::invalid_argument(message);
}

// ============== Finite ==============
// Purpose: Detect non-finite intermediate normalization and variance values.
// Inputs: Value and description. Outputs: Original value or an exception.
double Finite(double value, const char* description) {
  if(!std::isfinite(value))
    throw std::overflow_error(std::string(description) + " is not finite.");
  return value;
}

// ============== SignalWeights ==============
// Purpose: Recover one gate's source-bin coefficients for independent Sum components.
// Inputs: Actual gate and coefficient count. Outputs: Zero/one weights.
std::vector<double> SignalWeights(const GateBins& gate, std::size_t size) {
  std::vector<double> weights(size, 0.0);
  const int first = gate.underflow ? 0 : gate.first;
  const int last = gate.overflow ? static_cast<int>(size) - 1 : gate.last;
  std::fill(weights.begin() + first, weights.begin() + last + 1, 1.0);
  return weights;
}

// ============== Dot ==============
// Purpose: Integrate counts or width-weighted ordinates over selected source bins.
// Inputs: Spectrum, weights, and integration convention. Outputs: Finite integral.
double Dot(const TH1D& spectrum, const std::vector<double>& weights, FractionIntegral convention) {
  double sum = 0.0;
  for(std::size_t bin = 0; bin < weights.size(); ++bin) {
    if(weights[bin] == 0.0)
      continue;
    const double width = convention == FractionIntegral::WidthWeighted ?
      spectrum.GetXaxis()->GetBinWidth(static_cast<int>(bin)) : 1.0;
    sum += weights[bin] * spectrum.GetBinContent(static_cast<int>(bin)) * width;
  }
  return Finite(sum, "Gate integral");
}

// ============== SelectNormalRange ==============
// Purpose: Resolve explicit fit or normalization coordinates on a physical axis.
// Inputs: Axis and half-open bounds. Outputs: Actual normal-bin range.
GateBins SelectNormalRange(const TAxis& axis, double low, double high) {
  return ResolveGate({1, "Calculation range", low, high, true}, ReadEdges(axis), {});
}

// ============== EmptyLike ==============
// Purpose: Allocate a result histogram without inheriting ROOT directory ownership.
// Inputs: Source grid, suffix, and title. Outputs: Empty owned histogram.
std::unique_ptr<TH1D> EmptyLike(const TH1D& source, const char* suffix, const char* title) {
  return MakeSpectrum(std::string(source.GetName()) + suffix, title, *source.GetXaxis(),
    ReadEdges(*source.GetXaxis()), source.GetYaxis()->GetTitle());
}

// ============== EstimateProjectionBackground ==============
// Purpose: Import a background curve or run checked TSpectrum estimation on normal bins.
// Inputs: Total projection and settings. Outputs: Detached fixed background curve.
std::unique_ptr<TH1D> EstimateProjectionBackground(const TH1D& total, const BackgroundConfig& settings) {
  const int count = total.GetNbinsX();
  std::vector<double> values(count);
  if(!settings.projectionBackground.empty()) {
    Require(settings.projectionBackground.size() == static_cast<std::size_t>(count),
      "Imported projection background requires exactly N normal-bin counts.");
    values = settings.projectionBackground;
  } else {
    Require(settings.spectrumIterations >= 1 && settings.spectrumIterations <= (count - 1) / 2,
      "TSpectrum requires at least 2 * iterations + 1 normal bins.");
    for(int bin = 1; bin <= count; ++bin) {
      values[bin - 1] = total.GetBinContent(bin);
      Require(values[bin - 1] >= 0.0, "TSpectrum requires nonnegative total-projection bins.");
    }
    int direction = TSpectrum::kBackDecreasingWindow;
    int order = TSpectrum::kBackOrder2;
    int window = TSpectrum::kBackSmoothing3;
    bool smoothing = true;
    bool compton = false;
    std::string options = settings.spectrumOptions;
    std::transform(options.begin(), options.end(), options.begin(),
      [](unsigned char c) { return c == ',' ? ' ' : static_cast<char>(std::tolower(c)); });
    std::istringstream tokens(options);
    std::string token;
    while(tokens >> token) {
      if(token.rfind("kback", 0) == 0)
        token.erase(0, 1);
      if(token == "backincreasingwindow") direction = TSpectrum::kBackIncreasingWindow;
      else if(token == "backdecreasingwindow") direction = TSpectrum::kBackDecreasingWindow;
      else if(token == "backorder2") order = TSpectrum::kBackOrder2;
      else if(token == "backorder4") order = TSpectrum::kBackOrder4;
      else if(token == "backorder6") order = TSpectrum::kBackOrder6;
      else if(token == "backorder8") order = TSpectrum::kBackOrder8;
      else if(token == "nosmoothing") smoothing = false;
      else if(token == "backsmoothing3") window = TSpectrum::kBackSmoothing3;
      else if(token == "backsmoothing5") window = TSpectrum::kBackSmoothing5;
      else if(token == "backsmoothing7") window = TSpectrum::kBackSmoothing7;
      else if(token == "backsmoothing9") window = TSpectrum::kBackSmoothing9;
      else if(token == "backsmoothing11") window = TSpectrum::kBackSmoothing11;
      else if(token == "backsmoothing13") window = TSpectrum::kBackSmoothing13;
      else if(token == "backsmoothing15") window = TSpectrum::kBackSmoothing15;
      else if(token == "compton") compton = true;
      else throw std::invalid_argument("Unknown TSpectrum option: " + token);
    }
    TSpectrum estimator;
    const char* error = estimator.Background(values.data(), count, settings.spectrumIterations,
      direction, order, smoothing, window, compton);
    Require(!error, error ? std::string("TSpectrum: ") + error : std::string());
  }
  auto background = EmptyLike(total, "_estimate", "Estimated projection background");
  for(int bin = 1; bin <= count; ++bin) {
    Require(std::isfinite(values[bin - 1]) && values[bin - 1] >= 0.0,
      "Projection background counts must be finite and nonnegative.");
    background->SetBinContent(bin, values[bin - 1]);
  }
  background->ResetStats();
  return background;
}

struct LocalFit {
  std::array<double, 6> parameters{};
  double origin = 0.0;
  double low = 0.0;
  double high = 0.0;
  int status = -1;
  double chi2 = 0.0;
  int ndf = 0;
};

// ============== PolynomialIntegral ==============
// Purpose: Integrate the local polynomial exactly in shifted physical coordinates.
// Inputs: Fit and physical bin edges. Outputs: Integrated background ordinate.
double PolynomialIntegral(const LocalFit& fit, double low, double high) {
  const double a = low - fit.origin;
  const double b = high - fit.origin;
  return fit.parameters[3] * (b - a) + fit.parameters[4] * (b * b - a * a) / 2.0 +
    fit.parameters[5] * (b * b * b - a * a * a) / 3.0;
}

// ============== FitLocalBackground ==============
// Purpose: Fit a Gaussian with fixed linear or free quadratic background using local fitter state.
// Inputs: Total projection, signal bins, and settings. Outputs: Valid fitted baseline and peak shape.
LocalFit FitLocalBackground(const TH1D& total, const std::vector<double>& weights,
  const BackgroundConfig& settings) {
  const auto edges = ReadEdges(*total.GetXaxis());
  int first = 1;
  int last = total.GetNbinsX();
  while(first <= last && weights[first] == 0.0) ++first;
  while(last >= first && weights[last] == 0.0) --last;
  Require(first <= last, "Local background fitting requires normal signal bins.");
  for(int bin = first; bin <= last; ++bin)
    Require(weights[bin] > 0.0, "A local single-Gaussian baseline fit requires a connected signal interval; use Sum, Manual, or TSpectrum.");
  const double signalLow = edges[first - 1];
  const double signalHigh = edges[last];
  const double span = signalHigh - signalLow;
  GateBins range;
  if(settings.useFitRange)
    range = SelectNormalRange(*total.GetXaxis(), settings.fitLow, settings.fitHigh);
  else
    range = SelectNormalRange(*total.GetXaxis(), std::max(edges.front(), signalLow - 2.0 * span),
      std::min(edges.back(), signalHigh + 2.0 * span));
  Require(range.first <= first && range.last >= last, "Local fit range must contain its signal gate.");
  const bool density = settings.fractionIntegral == FractionIntegral::Counts;
  auto ordinate = [&](int bin) {
    return total.GetBinContent(bin) / (density ? edges[bin] - edges[bin - 1] : 1.0);
  };
  auto endpoint = [&](int middle) {
    double sum = 0.0;
    int count = 0;
    for(int bin = std::max(1, middle - 1); bin <= std::min(total.GetNbinsX(), middle + 1); ++bin) {
      sum += ordinate(bin);
      ++count;
    }
    return sum / count;
  };
  LocalFit fit;
  fit.low = range.low;
  fit.high = range.high;
  fit.origin = (range.low + range.high) / 2.0;
  const double left = endpoint(range.first);
  const double right = endpoint(range.last);
  fit.parameters[3] = (left + right) / 2.0;
  fit.parameters[4] = (right - left) / (range.high - range.low);
  int peakBin = first;
  for(int bin = first; bin <= last; ++bin)
    if(ordinate(bin) > ordinate(peakBin)) peakBin = bin;
  fit.parameters[0] = std::max(ordinate(peakBin) - fit.parameters[3], 1.0);
  fit.parameters[1] = total.GetXaxis()->GetBinCenter(peakBin);
  const double minSigma = std::max((edges[peakBin] - edges[peakBin - 1]) * 0.05, span * 1.0e-6);
  fit.parameters[2] = std::clamp(span / 2.355, minSigma * 2.0, (range.high - range.low) / 2.0);
  unsigned int dataSize = 0;
  for(int bin = range.first; bin <= range.last; ++bin)
    if(total.GetBinError(bin) > 0.0) ++dataSize;
  const bool fixedLine = settings.fractionMethod == FractionMethod::LinearFixed;
  Require(dataSize > (fixedLine ? 3u : 6u), "Local background fit has too few bins with nonzero errors.");
  auto chiSquare = [&](const double* p) {
    LocalFit current = fit;
    std::copy(p, p + 6, current.parameters.begin());
    double chi2 = 0.0;
    const double rootTwo = std::sqrt(2.0);
    for(int bin = range.first; bin <= range.last; ++bin) {
      const double error = total.GetBinError(bin);
      if(error <= 0.0) continue;
      const double low = edges[bin - 1];
      const double high = edges[bin];
      const double gaussian = p[0] * p[2] * std::sqrt(std::acos(-1.0) / 2.0) *
        (std::erf((high - p[1]) / (rootTwo * p[2])) -
         std::erf((low - p[1]) / (rootTwo * p[2])));
      double expected = gaussian + PolynomialIntegral(current, low, high);
      if(!density) expected /= high - low;
      const double residual = (total.GetBinContent(bin) - expected) / error;
      chi2 += residual * residual;
    }
    return chi2;
  };
  ROOT::Math::Functor function(chiSquare, 6);
  ROOT::Fit::Fitter fitter;
  fitter.Config().SetMinimizer("Minuit2", "Migrad");
  fitter.Config().MinimizerOptions().SetPrintLevel(-1);
  fitter.Config().MinimizerOptions().SetMaxFunctionCalls(20000);
  fitter.Config().SetParamsSettings(6, fit.parameters.data());
  fitter.Config().ParSettings(0).SetLowerLimit(0.0);
  fitter.Config().ParSettings(1).SetLimits(signalLow, signalHigh);
  fitter.Config().ParSettings(2).SetLimits(minSigma, range.high - range.low);
  if(fixedLine)
    for(int parameter = 3; parameter < 6; ++parameter) fitter.Config().ParSettings(parameter).Fix();
  const bool converged = fitter.FitFCN(function, nullptr, dataSize, 1);
  fit.status = fitter.Result().Status();
  Require(converged && fitter.Result().IsValid() && fit.status == 0,
    "Local background fit did not converge; adjust the fit range or estimator.");
  for(int parameter = 0; parameter < 6; ++parameter)
    fit.parameters[parameter] = Finite(fitter.Result().Parameter(parameter), "Fitted parameter");
  fit.chi2 = Finite(fitter.Result().MinFcnValue(), "Local fit chi-square");
  fit.ndf = static_cast<int>(dataSize) - static_cast<int>(fitter.Result().NFreeParameters());
  return fit;
}

// ============== TemplateWeights ==============
// Purpose: Build background masks by explicit bin selection, including weighted Anti gaps.
// Inputs: Component, signal bounds, matrix grid, and configuration. Outputs: Template weights and ranges.
std::vector<double> TemplateWeights(SubtractionComponent& component, const std::vector<GateBins>& gates,
  const TAxis& axis, const Config& config) {
  const int bins = axis.GetNbins();
  const auto& bg = config.background;
  const auto edges = ReadEdges(axis);
  const int first = config.flow.gateUnderflow ? 0 : 1;
  const int last = config.flow.gateOverflow ? bins + 1 : bins;
  std::vector<double> weights(bins + 2, 0.0);
  if(bg.selection == BackgroundTemplate::Full || bg.selection == BackgroundTemplate::Anti) {
    for(int bin = first; bin <= last; ++bin)
      weights[bin] = bg.selection == BackgroundTemplate::Anti && component.signalWeights[bin] > 0.0 ? 0.0 : 1.0;
  } else if(bg.selection == BackgroundTemplate::Compton) {
    int highest = 0;
    for(int bin = 1; bin <= bins; ++bin)
      if(component.signalWeights[bin] > 0.0) highest = bin;
    int start = highest + 1 + bg.comptonGapBins;
    if(component.fitStatus == 0) {
      const double bound = component.fittedCentroid + 3.0 * component.fittedSigma;
      const int fitted = static_cast<int>(std::upper_bound(edges.begin(), edges.end(), bound) - edges.begin());
      if(fitted >= highest) start = fitted;
    }
    for(int bin = std::max(start, 1); bin <= last; ++bin) weights[bin] = 1.0;
  } else {
    double anchorLow = std::numeric_limits<double>::infinity();
    double anchorHigh = -std::numeric_limits<double>::infinity();
    for(const auto& gate : gates) {
      anchorLow = std::min(anchorLow, gate.requested.low);
      anchorHigh = std::max(anchorHigh, gate.requested.high);
    }
    const double anchor = (anchorLow + anchorHigh) / 2.0;
    for(auto gate : bg.gates) {
      if(!gate.enabled) continue;
      if(bg.selection == BackgroundTemplate::ManualRelative) {
        gate.low += anchor;
        gate.high += anchor;
      }
      const auto actual = ResolveGate(gate, edges, config.flow);
      component.backgroundGates.push_back(actual);
      auto selected = SignalWeights(actual, weights.size());
      for(const auto& signal : gates) {
        const bool exclude = bg.manualOverlap == ManualOverlap::ExcludeSignal ||
          (bg.manualOverlap == ManualOverlap::Reference && signal.first > actual.first && signal.last < actual.last);
        if(exclude) {
          const auto signalMask = SignalWeights(signal, weights.size());
          for(std::size_t bin = 0; bin < weights.size(); ++bin)
            if(signalMask[bin] > 0.0) selected[bin] = 0.0;
        }
      }
      for(std::size_t bin = 0; bin < weights.size(); ++bin)
        weights[bin] = std::max(weights[bin], selected[bin]);
    }
  }
  Require(std::any_of(weights.begin(), weights.end(), [](double weight) { return weight > 0.0; }),
    "Background selection contains no usable source bins.");
  for(int bin = 1; bin <= bins; ++bin)
    component.backgroundWidth += weights[bin] * (edges[bin] - edges[bin - 1]);
  return weights;
}

// ============== RelativeUncertainty ==============
// Purpose: Resolve explicit uncertainty or the named reference-program preset.
// Inputs: Configuration. Outputs: Common relative subtraction-scale uncertainty.
double RelativeUncertainty(const Config& config) {
  if(config.background.relativeUncertainty >= 0.0) return config.background.relativeUncertainty;
  if(config.errors == ErrorPolicy::JRootCompatible)
    return config.background.fractionMethod == FractionMethod::Manual ? 0.08 : 0.04;
  if(config.errors == ErrorPolicy::RadWareCompatible) return 0.05;
  return 0.0;
}

// ============== BuildComponents ==============
// Purpose: Keep Sum subtraction arithmetic separate from Union normalization.
// Inputs: Projection and combination. Outputs: Source masks and stable gate identities.
void BuildComponents(Projection& projection, Combination combination) {
  if(combination == Combination::Sum) {
    for(const auto& gate : projection.gates) {
      SubtractionComponent component;
      component.gateIds = {gate.requested.id};
      component.signalWeights = SignalWeights(gate, projection.weights.size());
      projection.components.push_back(std::move(component));
    }
  } else {
    SubtractionComponent component;
    component.signalWeights = projection.weights;
    for(const auto& gate : projection.gates) component.gateIds.push_back(gate.requested.id);
    projection.components.push_back(std::move(component));
  }
}

// ============== ValidateSymmetric ==============
// Purpose: Restrict the RadWare model to explicitly identified symmetric gamma matrices.
// Inputs: Matrix and settings. Outputs: Exception for incompatible axes or data.
void ValidateSymmetric(const TH2& matrix, const BackgroundConfig& settings) {
  const auto x = ReadEdges(*matrix.GetXaxis());
  const auto y = ReadEdges(*matrix.GetYaxis());
  Require(x == y, "RadWare standard background requires identical X/Y bin edges.");
  for(int first = 1; first <= matrix.GetNbinsX(); ++first) {
    for(int second = first; second <= matrix.GetNbinsY(); ++second) {
      const double a = matrix.GetBinContent(first, second);
      const double b = matrix.GetBinContent(second, first);
      Require(std::abs(a - b) <= settings.symmetryTolerance * std::max({1.0, std::abs(a), std::abs(b)}),
        "RadWare standard background requires a symmetric matrix.");
    }
  }
}

} // namespace

// ============== ValidateBackgroundConfig ==============
// Purpose: Validate mathematical mode, masks, and uncertainty settings before projection.
// Inputs: Committed configuration. Outputs: Exception for inconsistent settings.
void ValidateBackgroundConfig(const Config& config) {
  const auto& bg = config.background;
  Require(bg.model == SubtractionModel::None || bg.model == SubtractionModel::Template ||
    bg.model == SubtractionModel::RadWareStandard, "Invalid subtraction model.");
  Require(static_cast<int>(bg.selection) >= 0 && static_cast<int>(bg.selection) <= 5,
    "Invalid background template.");
  Require(static_cast<int>(bg.fractionMethod) >= 0 && static_cast<int>(bg.fractionMethod) <= 4,
    "Invalid fraction estimator.");
  Require(static_cast<int>(config.errors) >= 0 && static_cast<int>(config.errors) <= 2,
    "Invalid error policy.");
  Require(bg.fractionIntegral == FractionIntegral::Counts || bg.fractionIntegral == FractionIntegral::WidthWeighted,
    "Invalid fraction integral convention.");
  Require(static_cast<int>(bg.manualOverlap) >= 0 && static_cast<int>(bg.manualOverlap) <= 2,
    "Invalid manual-overlap policy.");
  Require(std::isfinite(bg.fraction) && bg.fraction >= 0.0 && bg.fraction <= 1.0,
    "Manual background fraction must lie between 0 and 1.");
  Require(std::isfinite(bg.relativeUncertainty) && (bg.relativeUncertainty == -1.0 || bg.relativeUncertainty >= 0.0),
    "Relative background uncertainty must be -1 (preset) or nonnegative.");
  Require(bg.comptonGapBins >= 0, "Compton gap must be a nonnegative number of bins.");
  Require(std::isfinite(bg.symmetryTolerance) && bg.symmetryTolerance >= 0.0,
    "Symmetry tolerance must be finite and nonnegative.");
  std::set<std::uint64_t> ids;
  for(const auto& gate : bg.gates)
    Require(gate.id && ids.insert(gate.id).second, "Background gates require unique, nonzero IDs.");
  if(bg.model == SubtractionModel::None) {
    Require(bg.selection == BackgroundTemplate::None && bg.gates.empty() && bg.projectionBackground.empty() &&
      bg.fraction == 0.0 && (bg.relativeUncertainty == -1.0 || bg.relativeUncertainty == 0.0) &&
      config.errors == ErrorPolicy::FixedCoefficients,
      "Select a subtraction model before configuring backgrounds or compatibility errors.");
    return;
  }
  const bool flow = config.flow.gateUnderflow || config.flow.gateOverflow ||
    config.flow.resultUnderflow || config.flow.resultOverflow;
  if(bg.model == SubtractionModel::RadWareStandard) {
    Require(bg.symmetricGammaGamma, "Confirm symmetricGammaGamma before using the RadWare 2D model.");
    Require(bg.selection == BackgroundTemplate::None && bg.gates.empty() &&
      bg.fractionMethod == FractionMethod::TSpectrum && !bg.useNormalizationRange && !flow &&
      bg.fractionIntegral == FractionIntegral::Counts && config.errors != ErrorPolicy::JRootCompatible,
      "RadWare mode uses a full finite symmetric matrix, a 1D background curve, and fixed/RadWare errors.");
  } else {
    Require(config.errors != ErrorPolicy::RadWareCompatible, "RadWare errors require the RadWare model.");
    if(bg.selection == BackgroundTemplate::None)
      return;
    const bool manual = bg.selection == BackgroundTemplate::Manual || bg.selection == BackgroundTemplate::ManualRelative;
    Require(manual || bg.gates.empty(), "Explicit background windows require Manual or Manual Relative.");
    Require(!manual || std::any_of(bg.gates.begin(), bg.gates.end(), [](const Gate& gate) { return gate.enabled; }),
      "Manual background requires an enabled background window.");
    if(bg.fractionMethod == FractionMethod::Width || bg.fractionIntegral == FractionIntegral::WidthWeighted ||
       bg.fractionMethod == FractionMethod::LinearFixed || bg.fractionMethod == FractionMethod::PolynomialFlex ||
       bg.fractionMethod == FractionMethod::TSpectrum)
      Require(!config.flow.gateUnderflow && !config.flow.gateOverflow,
        "Width and estimated-fraction modes require finite normal gate bins.");
  }
}

// ============== ApplyBackground ==============
// Purpose: Calculate central values and conditional covariance for both subtraction models.
// Inputs: Matrix and owned projections. Outputs: Templates, subtraction, net spectra, and metadata.
void ApplyBackground(const TH2& matrix, Result& result) {
  const auto& config = result.config;
  const auto& settings = config.background;
  const bool enabled = settings.model == SubtractionModel::RadWareStandard ||
    (settings.model == SubtractionModel::Template && settings.selection != BackgroundTemplate::None);
  const bool radware = settings.model == SubtractionModel::RadWareStandard;
  const bool gateX = config.gateAxis == Axis::X;
  const int gateBins = result.total->GetNbinsX();
  if(radware) ValidateSymmetric(matrix, settings);
  if(enabled && (radware || settings.fractionMethod == FractionMethod::TSpectrum)) {
    result.estimatedBackground = EstimateProjectionBackground(*result.total, settings);
    result.backgroundSource = settings.projectionBackground.empty() ? "TSpectrum (fixed estimate)" : "Imported counts (fixed estimate)";
  }
  const double totalCounts = radware ? Finite(result.total->Integral(1, gateBins), "RadWare total counts") : 0.0;
  if(radware) {
    Require(totalCounts > 0.0, "RadWare normalization requires positive total matrix counts.");
    for(int bin = 1; bin <= gateBins; ++bin)
      Require(result.total->GetBinContent(bin) >= 0.0,
        "RadWare requires nonnegative total-projection counts.");
  }
  if(result.estimatedBackground) {
    for(int bin = 1; bin <= gateBins; ++bin) {
      const double excess = result.estimatedBackground->GetBinContent(bin) - result.total->GetBinContent(bin);
      if(excess > 0.0) {
        ++result.backgroundAboveTotalBins;
        result.maximumBackgroundExcess = std::max(result.maximumBackgroundExcess, excess);
      }
    }
  }
  const auto& resultAxis = *(gateX ? matrix.GetYaxis() : matrix.GetXaxis());
  int normalizationFirst = 1;
  int normalizationLast = resultAxis.GetNbins();
  if(enabled && settings.useNormalizationRange) {
    const auto range = SelectNormalRange(resultAxis, settings.normalizationLow, settings.normalizationHigh);
    normalizationFirst = range.first;
    normalizationLast = range.last;
  }
  std::vector<double> normalizationMarginal(gateBins + 2, 0.0);
  if(enabled && !radware) {
    const int firstGate = config.flow.gateUnderflow ? 0 : 1;
    const int lastGate = config.flow.gateOverflow ? gateBins + 1 : gateBins;
    for(int gateBin = firstGate; gateBin <= lastGate; ++gateBin) {
      if(!settings.useNormalizationRange && !config.flow.resultUnderflow && !config.flow.resultOverflow)
        normalizationMarginal[gateBin] = result.total->GetBinContent(gateBin);
      else {
        for(int bin = normalizationFirst; bin <= normalizationLast; ++bin)
          normalizationMarginal[gateBin] += gateX ? matrix.GetBinContent(gateBin, bin) : matrix.GetBinContent(bin, gateBin);
      }
    }
  }
  for(auto& projection : result.projections) {
    const int resultBins = projection.raw->GetNbinsX();
    const int firstResult = config.flow.resultUnderflow ? 0 : 1;
    const int lastResult = config.flow.resultOverflow ? resultBins + 1 : resultBins;
    projection.subtracted = EmptyLike(*projection.raw, "_subtracted", "Subtracted background");
    projection.net = EmptyLike(*projection.raw, "_net", "Net gated projection");
    projection.statisticalVariance.resize(resultBins + 2, 0.0);
    projection.systematicShift.resize(resultBins + 2, 0.0);
    projection.varianceFloor.resize(resultBins + 2, 0.0);
    projection.normalizationFirst = normalizationFirst;
    projection.normalizationLast = normalizationLast;
    projection.relativeUncertainty = enabled ? RelativeUncertainty(config) : 0.0;
    std::vector<double> correctionWeights(gateBins + 2, 0.0);
    std::vector<double> templateWeights(gateBins + 2, 0.0);
    double rwAlpha = 0.0;
    double rwBeta = 0.0;
    if(enabled) {
      BuildComponents(projection, config.combination);
      for(std::size_t index = 0; index < projection.components.size(); ++index) {
        auto& component = projection.components[index];
        std::vector<GateBins> gates = config.combination == Combination::Sum ?
          std::vector<GateBins>{projection.gates[index]} : projection.gates;
        if(radware) {
          const double gateCounts = Dot(*result.total, component.signalWeights, FractionIntegral::Counts);
          const double backgroundCounts = Dot(*result.estimatedBackground, component.signalWeights, FractionIntegral::Counts);
          component.rawIntegral = gateCounts;
          component.templateIntegral = totalCounts;
          component.fraction = gateCounts > 0.0 ? backgroundCounts / gateCounts : 0.0;
          component.fractionDefined = gateCounts > 0.0;
          component.unclampedFraction = component.fraction;
          component.alpha = backgroundCounts / totalCounts;
          component.beta = (gateCounts - backgroundCounts) / totalCounts;
          rwAlpha += component.alpha;
          rwBeta += component.beta;
          continue;
        }
        LocalFit localFit;
        if(settings.fractionMethod == FractionMethod::LinearFixed || settings.fractionMethod == FractionMethod::PolynomialFlex) {
          localFit = FitLocalBackground(*result.total, component.signalWeights, settings);
          component.fitLow = localFit.low;
          component.fitHigh = localFit.high;
          component.fitStatus = localFit.status;
          component.fittedCentroid = localFit.parameters[1];
          component.fittedSigma = localFit.parameters[2];
          component.fitParameters = localFit.parameters;
          component.fitOrigin = localFit.origin;
          component.fitChi2 = localFit.chi2;
          component.fitNdf = localFit.ndf;
        }
        component.templateWeights = TemplateWeights(component, gates, *result.total->GetXaxis(), config);
        for(int bin = 0; bin <= gateBins + 1; ++bin) {
          component.rawIntegral += component.signalWeights[bin] * normalizationMarginal[bin];
          component.templateIntegral += component.templateWeights[bin] * normalizationMarginal[bin];
        }
        Finite(component.rawIntegral, "Signal normalization integral");
        Require(std::isfinite(component.templateIntegral) && component.templateIntegral > 0.0,
          "Background normalization requires a positive template integral.");
        if(settings.fractionMethod == FractionMethod::Width) {
          double signalWidth = 0.0;
          for(int bin = 1; bin <= gateBins; ++bin)
            signalWidth += component.signalWeights[bin] * result.total->GetXaxis()->GetBinWidth(bin);
          Require(component.backgroundWidth > 0.0, "Width normalization requires positive finite background width.");
          component.alpha = signalWidth / component.backgroundWidth;
          component.fraction = component.rawIntegral > 0.0 ?
            component.alpha * component.templateIntegral / component.rawIntegral : 0.0;
          component.fractionDefined = component.rawIntegral > 0.0;
          component.unclampedFraction = component.fraction;
        } else {
          Require(component.rawIntegral > 0.0, "Fraction normalization requires positive signal counts in its range.");
          double fraction = settings.fraction;
          if(settings.fractionMethod != FractionMethod::Manual) {
            const double denominator = Dot(*result.total, component.signalWeights, settings.fractionIntegral);
            Require(denominator > 0.0, "Fraction estimate requires positive gated total-projection counts.");
            double numerator = 0.0;
            if(settings.fractionMethod == FractionMethod::TSpectrum)
              numerator = Dot(*result.estimatedBackground, component.signalWeights, settings.fractionIntegral);
            else {
              for(int bin = 1; bin <= gateBins; ++bin) {
                if(component.signalWeights[bin] == 0.0) continue;
                numerator += component.signalWeights[bin] * PolynomialIntegral(localFit,
                  result.total->GetXaxis()->GetBinLowEdge(bin), result.total->GetXaxis()->GetBinUpEdge(bin));
              }
            }
            fraction = Finite(numerator / denominator, "Estimated background fraction");
          }
          component.unclampedFraction = fraction;
          component.fraction = std::clamp(fraction, 0.0, 1.0);
          component.fractionClamped = fraction != component.fraction;
          component.alpha = component.fraction * component.rawIntegral / component.templateIntegral;
        }
        Finite(component.alpha, "Background scale");
        for(int bin = 0; bin <= gateBins + 1; ++bin) {
          correctionWeights[bin] += component.alpha * component.templateWeights[bin];
          templateWeights[bin] += component.templateWeights[bin];
        }
      }
    }
    if(enabled && !radware)
      projection.backgroundTemplate = EmptyLike(*projection.raw, "_template", "Unscaled background template sum");
    for(int bin = firstResult; bin <= lastResult; ++bin) {
      double correction = 0.0;
      double correctionVariance = 0.0;
      double netVariance = std::pow(projection.raw->GetBinError(bin), 2);
      if(radware) {
        correction = rwAlpha * result.total->GetBinContent(bin) +
          rwBeta * result.estimatedBackground->GetBinContent(bin);
      } else if(enabled) {
        double shape = 0.0;
        double shapeVariance = 0.0;
        netVariance = 0.0;
        for(int gateBin = 0; gateBin <= gateBins + 1; ++gateBin) {
          const double signal = projection.weights[gateBin];
          const double subtract = correctionWeights[gateBin];
          const double templateWeight = templateWeights[gateBin];
          if(signal == 0.0 && subtract == 0.0 && templateWeight == 0.0) continue;
          const int cell = gateX ? matrix.GetBin(gateBin, bin) : matrix.GetBin(bin, gateBin);
          const double value = matrix.GetBinContent(cell);
          const double variance = std::pow(matrix.GetBinError(cell), 2);
          correction += subtract * value;
          correctionVariance += subtract * subtract * variance;
          shape += templateWeight * value;
          shapeVariance += templateWeight * templateWeight * variance;
          const double coefficient = signal - subtract;
          netVariance += (config.errors == ErrorPolicy::JRootCompatible ?
            signal * signal + subtract * subtract : coefficient * coefficient) * variance;
        }
        projection.backgroundTemplate->SetBinContent(bin, Finite(shape, "Template content"));
        projection.backgroundTemplate->SetBinError(bin, std::sqrt(Finite(shapeVariance, "Template variance")));
      }
      const double shift = Finite(projection.relativeUncertainty * correction, "Scale uncertainty");
      projection.statisticalVariance[bin] = Finite(netVariance, "Net statistical variance");
      projection.systematicShift[bin] = shift;
      double variance = Finite(netVariance + shift * shift, "Net variance");
      if(config.errors == ErrorPolicy::RadWareCompatible && variance < 1.0) {
        projection.varianceFloor[bin] = 1.0 - variance;
        variance = 1.0;
      }
      projection.subtracted->SetBinContent(bin, Finite(correction, "Subtracted background"));
      projection.subtracted->SetBinError(bin, std::sqrt(Finite(correctionVariance + shift * shift, "Background variance")));
      projection.net->SetBinContent(bin, Finite(projection.raw->GetBinContent(bin) - correction, "Net content"));
      projection.net->SetBinError(bin, std::sqrt(variance));
    }
    if(projection.backgroundTemplate) projection.backgroundTemplate->ResetStats();
    projection.subtracted->ResetStats();
    projection.net->ResetStats();
  }
}

} // namespace MatrixGate::Detail

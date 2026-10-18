#ifndef MATRIXGATE_RESULT_H
#define MATRIXGATE_RESULT_H

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <TH1D.h>
#include <MatrixGate/MatrixGateConfig.h>

namespace MatrixGate {

struct GateBins {
  Gate requested;
  int first = 0;
  int last = 0;
  double low = 0.0;
  double high = 0.0;
  double width = 0.0; // Normal-bin physical width; flow bins have no finite width.
  bool underflow = false;
  bool overflow = false;
};

struct SubtractionComponent {
  std::vector<std::uint64_t> gateIds;
  std::vector<GateBins> backgroundGates;
  std::vector<double> signalWeights;
  std::vector<double> templateWeights;
  double fraction = 0.0;
  bool fractionDefined = true;
  double unclampedFraction = 0.0;
  bool fractionClamped = false;
  double alpha = 0.0;
  double beta = 0.0; // RadWare coefficient multiplying the estimated 1D background.
  double rawIntegral = 0.0;
  double templateIntegral = 0.0;
  double backgroundWidth = 0.0;
  double fitLow = 0.0;
  double fitHigh = 0.0;
  double fittedCentroid = 0.0;
  double fittedSigma = 0.0;
  int fitStatus = -1;
  std::array<double, 6> fitParameters{};
  double fitOrigin = 0.0;
  double fitChi2 = 0.0;
  int fitNdf = 0;
};

struct Projection {
  std::vector<GateBins> gates;
  // Indexed like the source gate axis, including indices 0 and N+1.
  std::vector<double> weights;
  double width = 0.0;
  std::unique_ptr<TH1D> raw;
  std::unique_ptr<TH1D> backgroundTemplate;
  std::unique_ptr<TH1D> subtracted;
  std::unique_ptr<TH1D> net;
  std::vector<SubtractionComponent> components;
  std::vector<double> statisticalVariance;
  // One common relative subtraction-scale nuisance; its covariance is shift_i * shift_j.
  std::vector<double> systematicShift;
  std::vector<double> varianceFloor;
  double relativeUncertainty = 0.0;
  int normalizationFirst = 0;
  int normalizationLast = 0;

  // ============== Covariance ==============
  // Purpose: Return the stored conditional net covariance, including the common scale nuisance.
  // Inputs: ROOT bin indices including flow bins. Outputs: Covariance or out_of_range.
  double Covariance(int first, int second) const;
};

struct Result {
  bool success = false;
  std::string message;
  std::string sourceName;
  std::string sourceTitle;
  Config config;
  // The total is on the gate axis; each raw projection is on the other axis.
  std::unique_ptr<TH1D> total;
  std::unique_ptr<TH1D> estimatedBackground;
  std::string backgroundSource;
  int backgroundAboveTotalBins = 0;
  double maximumBackgroundExcess = 0.0;
  std::vector<Projection> projections;

  // ============== Report ==============
  // Purpose: Describe the source, actual gates, flow policy, and error convention.
  // Inputs: This calculation result. Outputs: English text without side effects.
  std::string Report() const;
};

} // namespace MatrixGate

#endif

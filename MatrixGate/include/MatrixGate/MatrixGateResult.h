#ifndef MATRIXGATE_RESULT_H
#define MATRIXGATE_RESULT_H

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

struct Projection {
  std::vector<GateBins> gates;
  // Indexed like the source gate axis, including indices 0 and N+1.
  std::vector<double> weights;
  double width = 0.0;
  std::unique_ptr<TH1D> raw;
};

struct Result {
  bool success = false;
  std::string message;
  std::string sourceName;
  std::string sourceTitle;
  Config config;
  // The total is on the gate axis; each raw projection is on the other axis.
  std::unique_ptr<TH1D> total;
  std::vector<Projection> projections;

  // ============== Report ==============
  // Purpose: Describe the source, actual gates, flow policy, and error convention.
  // Inputs: This calculation result. Outputs: English text without side effects.
  std::string Report() const;
};

} // namespace MatrixGate

#endif

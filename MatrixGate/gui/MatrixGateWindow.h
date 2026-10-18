#ifndef MATRIXGATE_WINDOW_H
#define MATRIXGATE_WINDOW_H

#include <memory>
#include <TGFrame.h>
#include <MatrixGate/MatrixGateConfig.h>

class TCanvas;
class TGTextEntry;

namespace MatrixGate::Detail {

class Session;

enum WidgetId {
  kUpdate = 100, kExit, kAuto, kAxis, kCombination, kResult, kPick, kBrowse,
  kRaw, kSubtracted, kNet, kAddSignal, kAddBackground, kDelete, kGateList,
  kEnabled, kLow, kHigh, kCenter, kWidth, kSlider, kName,
  kModel, kTemplate, kFractionMethod, kFraction, kUncertainty, kErrors,
  kIntegral, kOverlap, kIterations, kOptions, kComptonGap, kSymmetric,
  kUseFitRange, kFitLow, kFitHigh, kUseNormRange, kNormLow, kNormHigh,
  kAutomaticCurve, kGateUnder, kGateOver, kResultUnder, kResultOver,
  kTotalRebin, kTotalUndo, kResultRebin, kResultUndo
};

class Window : public TGMainFrame {
public:
  explicit Window(Session* session);
  ~Window() override;
  Bool_t ProcessMessage(Long_t message, Long_t parameter1, Long_t parameter2) override;
  void CloseWindow() override;
  bool Submit(bool calculate);
  void Refresh(bool discardEdits = false);
  void Show();
  void Detach();
  bool Dirty() const;
  TCanvas* TotalCanvas() const;
  TCanvas* ResultCanvas() const;
  void ForgetGate(Axis axis, bool background, std::uint64_t id);
  void CollectRows(bool all = false);

private:
  class Impl;
  std::unique_ptr<Impl> fImpl;
};

} // namespace MatrixGate::Detail

#endif

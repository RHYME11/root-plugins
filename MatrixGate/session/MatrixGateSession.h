#ifndef MATRIXGATE_SESSION_H
#define MATRIXGATE_SESSION_H

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <MatrixGate/MatrixGateResult.h>

class TH2;
class TCanvas;
class TLine;
class TObject;
class TLegend;
class TPaveText;

namespace MatrixGate::Detail {

class Window;

class Session {
public:
  Session(TH2* source, Config config, Result initial, bool openWindow = true);
  ~Session();
  bool Configure(const Config& config);
  bool Refresh(bool resetView = false);
  void DraftChanged();
  void Tick();
  void Close(bool sourceDeleted = false);
  void Raise();
  void SetStatus(const std::string& status);
  void AddGate(bool background, double center = 0.0, bool explicitCenter = false);
  void DeleteSelected();
  void Select(bool background, std::uint64_t id);
  bool SetBounds(double low, double high, bool preview = false);
  void SwitchAxis(Axis axis);
  void SetAuto(bool enabled);
  void SetBrowse(bool browse);
  void SetView(bool raw, bool background, bool net);
  void SetResultIndex(std::size_t index);
  void CanvasEvent(TCanvas* canvas, int event, int px, int py, TObject* selected);
  void Render(bool resetView = false);
  void Preview();
  TH1D* Display(TCanvas* canvas) const;
  void Pan(TCanvas* canvas, int direction);
  void Unzoom(TCanvas* canvas);
  void Rebin(TCanvas* canvas, bool undo);
  int RebinFactor(bool total) const;
  bool SyncResultView(bool force = false);
  void SetGesture(bool active);
  Gate* SelectedGate();
  const Gate* SelectedGate() const;
  double Anchor() const;
  std::vector<double> GateEdges() const;
  std::pair<double, double> VisibleRange() const;
  std::pair<double, double> GateLimits(bool background) const;
  bool SyncView(bool redraw = true);
  int GateColor(std::uint64_t id) const;

  // ============== Source ==============
  // Purpose: Read the borrowed source matrix. Inputs: None. Outputs: Borrowed state/value.
  TH2* Source() const { return fSource; }
  // ============== Configuration ==============
  // Purpose: Read committed settings. Inputs: None. Outputs: Borrowed state/value.
  const Config& Configuration() const { return fConfig; }
  // ============== CurrentResult ==============
  // Purpose: Read the last successful calculation snapshot. Inputs: None. Outputs: Borrowed state/value.
  const Result& CurrentResult() const { return fResult; }
  // ============== Status ==============
  // Purpose: Read the visible session diagnostic. Inputs: None. Outputs: Borrowed state/value.
  const std::string& Status() const { return fStatus; }
  // ============== Closed ==============
  // Purpose: Read the shutdown flag. Inputs: None. Outputs: Borrowed state/value.
  bool Closed() const { return fClosed; }
  // ============== Current ==============
  // Purpose: Read snapshot validity for current settings. Inputs: None. Outputs: Borrowed state/value.
  bool Current() const { return fCurrent; }
  // ============== Auto ==============
  // Purpose: Read the automatic update flag. Inputs: None. Outputs: Borrowed state/value.
  bool Auto() const { return fAuto; }
  // ============== Browse ==============
  // Purpose: Read the native browsing mode. Inputs: None. Outputs: Borrowed state/value.
  bool Browse() const { return fBrowse; }
  // ============== SelectedBackground ==============
  // Purpose: Read the selected gate kind. Inputs: None. Outputs: Borrowed state/value.
  bool SelectedBackground() const { return fSelectedBackground; }
  // ============== SelectedId ==============
  // Purpose: Read the stable gate identity. Inputs: None. Outputs: Borrowed state/value.
  std::uint64_t SelectedId() const { return fSelectedId; }
  // ============== ResultIndex ==============
  // Purpose: Read the shown snapshot index. Inputs: None. Outputs: Borrowed state/value.
  std::size_t ResultIndex() const { return fResultIndex; }
  // ============== ControlWindow ==============
  // Purpose: Read the borrowed control window. Inputs: None. Outputs: Borrowed state/value.
  Window* ControlWindow() const { return fWindow.get(); }
  TCanvas* TotalCanvas() const;
  TCanvas* ResultCanvas() const;
  // ============== TotalDisplay ==============
  // Purpose: Read the detached total display clone. Inputs: None. Outputs: Borrowed state/value.
  TH1D* TotalDisplay() const { return fTotalDisplay.get(); }

private:
  friend class Window;
  struct Marker {
    std::unique_ptr<TLine> line;
    std::uint64_t id = 0;
    bool background = false;
    bool upper = false;
    double anchor = 0.0;
    std::uint64_t anchorId = 0;
  };
  void Changed();
  void ClearDrawings();
  void DrawMarkers();
  void RepaintStatus();
  void ConstrainGates(Config& config, std::pair<double, double> oldRange,
    std::pair<double, double> newRange) const;
  std::vector<double> Anchors(const Config& config) const;
  TH2* fSource = nullptr;
  Config fConfig;
  std::array<Config, 2> fAxisConfigs;
  std::array<std::pair<double, double>, 2> fViews{};
  std::array<std::array<int, 2>, 2> fRebinFactors{{{{1, 1}}, {{1, 1}}}};
  std::pair<double, double> fObservedResultRange{0.0, 0.0};
  std::pair<double, double> fAutoResultY{0.0, 0.0};
  std::pair<double, double> fManualResultYRange{0.0, 1.0};
  bool fManualResultY = false;
  bool fObservedLogY = false;
  Result fResult;
  std::unique_ptr<Window> fWindow;
  std::string fStatus;
  std::uint64_t fNextId = 1;
  std::uint64_t fSelectedId = 0;
  std::size_t fResultIndex = 0;
  bool fSelectedBackground = false;
  bool fClosed = false;
  bool fCurrent = true;
  bool fAuto = true;
  bool fBrowse = false;
  bool fGesture = false;
  bool fPending = false;
  unsigned long long fDue = 0;
  bool fShowRaw = false;
  bool fShowBackground = false;
  bool fShowNet = true;
  bool fClick = false;
  bool fIgnoreUp = false;
  int fDownX = 0;
  int fDownY = 0;
  bool fFirstBound = false;
  double fFirstX = 0.0;
  int fDragging = -1;
  std::unique_ptr<TH1D> fTotalDisplay;
  std::unique_ptr<TH1D> fEstimateDisplay;
  std::array<std::unique_ptr<TH1D>, 3> fDisplays;
  std::vector<Marker> fMarkers;
  std::unique_ptr<TLegend> fLegend;
  std::unique_ptr<TPaveText> fLeftStatus;
  std::unique_ptr<TPaveText> fRightStatus;
};

} // namespace MatrixGate::Detail

#endif

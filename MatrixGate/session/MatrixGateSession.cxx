#include "MatrixGateSession.h"
#include "MatrixGateWindow.h"
#include "MatrixGateProjection.h"
#include <MatrixGate/MatrixGateRoot.h>
#include <MatrixGate/detail/MatrixGateRootController.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <Buttons.h>
#include <KeySymbols.h>
#include <TCanvas.h>
#include <TColor.h>
#include <TDirectory.h>
#include <TFrame.h>
#include <TH2.h>
#include <TLegend.h>
#include <TLine.h>
#include <TList.h>
#include <TPaveText.h>
#include <TROOT.h>
#include <TSystem.h>

namespace MatrixGate::Detail {
namespace {

constexpr UInt_t kGateMarker = 0x4d470001;
constexpr const char* kEvent = "ProcessedEvent(Int_t,Int_t,Int_t,TObject*)";
constexpr const char* kReceiver = "MatrixGate::Detail::RootController";

// ============== Alive ==============
// Purpose: Check canvas identity before dereferencing it. Inputs: Borrowed canvas. Outputs: Liveness.
bool Alive(TCanvas* canvas) {
  return canvas && gROOT && gROOT->GetListOfCanvases()->FindObject(canvas);
}

// ============== Copy ==============
// Purpose: Separate native display edits from calculation snapshots. Inputs: Spectrum. Outputs: Detached clone.
std::unique_ptr<TH1D> Copy(const TH1D& source) {
  TDirectory::TContext context(nullptr);
  auto output = std::unique_ptr<TH1D>(static_cast<TH1D*>(source.Clone((std::string(source.GetName()) + "_view").c_str())));
  output->SetDirectory(nullptr);
  output->SetStats(false);
  return output;
}

struct View {
  bool ranged = false;
  double low = 0.0;
  double high = 0.0;
};

// ============== ReadView ==============
// Purpose: Remember native axis zoom across redraws. Inputs: Display histogram. Outputs: Physical bounds.
View ReadView(const TH1D* histogram) {
  if(!histogram) return {};
  const auto* axis = histogram->GetXaxis();
  return {axis->TestBit(TAxis::kAxisRange), axis->GetBinLowEdge(axis->GetFirst()), axis->GetBinUpEdge(axis->GetLast())};
}

// ============== RestoreView ==============
// Purpose: Restore a compatible native zoom. Inputs: Spectrum and previous bounds. Outputs: Updated axis.
void RestoreView(TH1D& histogram, const View& view) {
  if(view.ranged && view.low < view.high)
    histogram.GetXaxis()->SetRangeUser(view.low, view.high);
}

// ============== Stamp ==============
// Purpose: Place an owned status label without changing native canvas interaction.
// Inputs: Label and normalized position. Outputs: Non-pickable status object.
std::unique_ptr<TPaveText> Stamp(const char* message, double low = 0.82) {
  auto text = std::make_unique<TPaveText>(0.15, low, 0.88, low + 0.08, "NDC");
  text->SetFillStyle(0);
  text->SetBorderSize(0);
  text->SetTextColor(kRed + 1);
  text->SetTextSize(0.035);
  text->SetBit(TObject::kCannotPick);
  text->AddText(message);
  return text;
}

} // namespace

// ============== Session ==============
// Purpose: Own one matrix analysis without owning its source. Inputs: Source/config/result. Outputs: Live session.
Session::Session(TH2* source, Config config, Result initial, bool openWindow)
  : fSource(source), fConfig(std::move(config)), fResult(std::move(initial)) {
  if(fConfig.background.model == SubtractionModel::None) {
    fConfig.background.model = SubtractionModel::Template;
    fConfig.background.selection = BackgroundTemplate::None;
  }
  for(int axis = 0; axis < 2; ++axis) {
    fAxisConfigs[axis].gateAxis = axis == 0 ? Axis::X : Axis::Y;
    fAxisConfigs[axis].background.model = SubtractionModel::Template;
  }
  for(const auto* gates : {&fConfig.gates, &fConfig.background.gates})
    for(const auto& gate : *gates) fNextId = std::max(fNextId, gate.id + 1);
  if(!fConfig.gates.empty()) fSelectedId = fConfig.gates.front().id;
  for(int index = 0; index < 2; ++index) {
    const auto* axis = index == 0 ? source->GetXaxis() : source->GetYaxis();
    fViews[index] = {axis->GetXmin(), axis->GetXmax()};
  }
  fStatus = "Ready. Pick two edges or double-click a center on the total projection.";
  if(openWindow) {
    fWindow = std::make_unique<Window>(this);
    TotalCanvas()->Connect(kEvent, kReceiver, &RootController::Instance(),
      "OnCanvasEvent(Int_t,Int_t,Int_t,TObject*)");
    fWindow->Show();
    Render();
  }
}

// ============== ~Session ==============
// Purpose: Detach callbacks before deferred native deletion. Inputs: Owned state. Outputs: Released drawings/window.
Session::~Session() {
  Close();
  ClearDrawings();
  if(fWindow) {
    auto* window = fWindow.release();
    window->Detach();
    window->DeleteWindow();
  }
}

// ============== TotalCanvas ==============
// Purpose: Locate the gate canvas. Inputs: None. Outputs: Borrowed canvas or null.
TCanvas* Session::TotalCanvas() const { return fWindow ? fWindow->TotalCanvas() : nullptr; }

// ============== ResultCanvas ==============
// Purpose: Locate the result canvas. Inputs: None. Outputs: Borrowed canvas or null.
TCanvas* Session::ResultCanvas() const { return fWindow ? fWindow->ResultCanvas() : nullptr; }

// ============== SelectedGate ==============
// Purpose: Resolve stable selection after edits or sorting. Inputs: None. Outputs: Borrowed gate.
Gate* Session::SelectedGate() {
  auto& gates = fSelectedBackground ? fConfig.background.gates : fConfig.gates;
  for(auto& gate : gates) if(gate.id == fSelectedId) return &gate;
  return nullptr;
}

// ============== SelectedGate ==============
// Purpose: Read stable selection. Inputs: None. Outputs: Borrowed const gate.
const Gate* Session::SelectedGate() const { return const_cast<Session*>(this)->SelectedGate(); }

// ============== GateEdges ==============
// Purpose: Read the active source grid. Inputs: None. Outputs: Gate-axis edges.
std::vector<double> Session::GateEdges() const {
  if(!fSource) return {};
  return ReadEdges(*(fConfig.gateAxis == Axis::X ? fSource->GetXaxis() : fSource->GetYaxis()));
}

// ============== Anchor ==============
// Purpose: Position relative background windows for the shown selection. Inputs: None. Outputs: Physical midpoint.
double Session::Anchor() const {
  double low = std::numeric_limits<double>::infinity();
  double high = -low;
  std::size_t index = 0;
  for(const auto& gate : fConfig.gates) {
    if(!gate.enabled) continue;
    if(fConfig.combination == Combination::Separate && index++ == fResultIndex)
      return (gate.low + gate.high) / 2.0;
    low = std::min(low, gate.low);
    high = std::max(high, gate.high);
  }
  if(std::isfinite(low) && std::isfinite(high)) return (low + high) / 2.0;
  const auto range = VisibleRange();
  return (range.first + range.second) / 2.0;
}

// ============== VisibleRange ==============
// Purpose: Read the remembered physical zoom. Inputs: None. Outputs: Visible low/high edges.
std::pair<double, double> Session::VisibleRange() const {
  return fViews[fConfig.gateAxis == Axis::X ? 0 : 1];
}

// ============== Anchors ==============
// Purpose: Locate every physical instance of relative windows. Inputs: Config. Outputs: Signal centers.
std::vector<double> Session::Anchors(const Config& config) const {
  std::vector<double> centers;
  double low = std::numeric_limits<double>::infinity();
  double high = -low;
  for(const auto& gate : config.gates) if(gate.enabled) {
    centers.push_back((gate.low + gate.high) / 2.0);
    low = std::min(low, gate.low); high = std::max(high, gate.high);
  }
  if(config.combination == Combination::Union && !centers.empty()) centers = {(low + high) / 2.0};
  if(centers.empty()) {
    const auto range = VisibleRange();
    centers = {(range.first + range.second) / 2.0};
  }
  return centers;
}

// ============== GateLimits ==============
// Purpose: Bound absolute or relative edits by all physical gate instances. Inputs: Gate kind. Outputs: Allowed coordinates.
std::pair<double, double> Session::GateLimits(bool background) const {
  auto range = VisibleRange();
  if(background && fConfig.background.selection == BackgroundTemplate::ManualRelative) {
    const auto centers = Anchors(fConfig);
    range.first -= *std::min_element(centers.begin(), centers.end());
    range.second -= *std::max_element(centers.begin(), centers.end());
  }
  return range;
}

// ============== ConstrainGates ==============
// Purpose: Preserve visible gates and proportionally remap only out-of-view gates.
// Inputs: Mutable config and old/new zoom. Outputs: Every physical gate remains visible.
void Session::ConstrainGates(Config& config, std::pair<double, double> oldRange,
  std::pair<double, double> newRange) const {
  auto oldCenters = Anchors(config);
  const bool hasSignal = std::any_of(config.gates.begin(), config.gates.end(), [](const Gate& gate) { return gate.enabled; });
  if(!hasSignal) oldCenters = {(oldRange.first + oldRange.second) / 2.0};
  auto constrain = [](Gate& gate, std::pair<double, double> from, std::pair<double, double> to) {
    if(gate.low >= to.first && gate.high <= to.second) return;
    const double scale = (to.second - to.first) / (from.second - from.first);
    double low = to.first + (gate.low - from.first) * scale;
    double high = to.first + (gate.high - from.first) * scale;
    const double width = std::min(high - low, to.second - to.first);
    low = std::clamp(low, to.first, to.second - width);
    gate.low = low; gate.high = low + width;
  };
  for(auto& gate : config.gates) constrain(gate, oldRange, newRange);
  if(config.background.selection == BackgroundTemplate::ManualRelative) {
    auto centers = Anchors(config);
    if(!hasSignal) centers = {(newRange.first + newRange.second) / 2.0};
    oldRange.first -= *std::min_element(oldCenters.begin(), oldCenters.end());
    oldRange.second -= *std::max_element(oldCenters.begin(), oldCenters.end());
    newRange.first -= *std::min_element(centers.begin(), centers.end());
    newRange.second -= *std::max_element(centers.begin(), centers.end());
  }
  for(auto& gate : config.background.gates) constrain(gate, oldRange, newRange);
}

// ============== SyncView ==============
// Purpose: Observe native axis zoom after ROOT finishes processing it. Inputs: Display axis. Outputs: Synchronized gates/sliders.
bool Session::SyncView(bool redraw) {
  if(fClosed || fGesture || !fTotalDisplay || fResult.config.gateAxis != fConfig.gateAxis) return false;
  const auto view = ReadView(fTotalDisplay.get());
  const auto edges = GateEdges();
  const std::pair<double, double> range{std::max(edges.front(), view.low), std::min(edges.back(), view.high)};
  auto& previous = fViews[fConfig.gateAxis == Axis::X ? 0 : 1];
  if(range == previous || range.first >= range.second) return false;
  const auto old = fConfig;
  ConstrainGates(fConfig, previous, range);
  previous = range;
  bool changed = false;
  for(int kind = 0; kind < 2; ++kind) {
    const auto& before = kind ? old.background.gates : old.gates;
    const auto& after = kind ? fConfig.background.gates : fConfig.gates;
    for(std::size_t i = 0; i < after.size(); ++i)
      changed = changed || before[i].low != after[i].low || before[i].high != after[i].high;
  }
  fFirstBound = false;
  if(changed) Changed();
  if(redraw) {
    Preview();
    if(fWindow) fWindow->Refresh();
  }
  return true;
}

// ============== GateColor ==============
// Purpose: Keep gate colors stable across deletions. Inputs: Stable ID. Outputs: ROOT color index.
int Session::GateColor(std::uint64_t id) const {
  static const int colors[] = {kBlue + 1, kGreen + 2, kMagenta + 1, kOrange + 7,
    kCyan + 2, kRed + 1, kViolet + 1, kTeal + 3, kPink + 7, kAzure + 7, kSpring - 5, kOrange + 2};
  return colors[(id ? id - 1 : 0) % (sizeof(colors) / sizeof(colors[0]))];
}

// ============== SetStatus ==============
// Purpose: Display a diagnostic without overwriting edits. Inputs: Message. Outputs: Updated status.
void Session::SetStatus(const std::string& status) {
  fStatus = status;
  if(fWindow) fWindow->Refresh();
}

// ============== Changed ==============
// Purpose: Mark the snapshot stale and schedule a bounded update. Inputs: Current settings. Outputs: Pending calculation.
void Session::Changed() {
  fCurrent = false;
  fPending = fAuto;
  fDue = static_cast<unsigned long long>(gSystem->Now()) + 250;
  SetStatus("Input changed. Last successful result is OUT OF DATE; choose Update.");
  RepaintStatus();
}

// ============== DraftChanged ==============
// Purpose: Protect incomplete GUI edits from being mistaken for a current result.
// Inputs: Draft notification. Outputs: Stale result and optional deferred commit.
void Session::DraftChanged() { if(!fClosed) Changed(); }

// ============== Configure ==============
// Purpose: Commit syntactically valid state while preserving the last successful result.
// Inputs: Candidate settings. Outputs: Acceptance or retained GUI draft and diagnostic.
bool Session::Configure(const Config& config) {
  if(fClosed || !fSource) return false;
  try {
    if(config.gateAxis != fConfig.gateAxis) throw std::invalid_argument("Use the axis selector to change axes.");
    const auto edges = GateEdges();
    for(const auto* gates : {&config.gates, &config.background.gates}) {
      std::set<std::uint64_t> ids;
      for(const auto& gate : *gates) {
        if(!gate.id || !ids.insert(gate.id).second) throw std::invalid_argument("Gate IDs must be unique and nonzero.");
        if(!std::isfinite(gate.low) || !std::isfinite(gate.high) || gate.low >= gate.high)
          throw std::invalid_argument("Gate bounds require finite Low < High.");
        auto limits = VisibleRange();
        if(gates == &config.background.gates && config.background.selection == BackgroundTemplate::ManualRelative) {
          const auto centers = Anchors(config);
          limits.first -= *std::min_element(centers.begin(), centers.end());
          limits.second -= *std::max_element(centers.begin(), centers.end());
        }
        if(gate.low < limits.first - 1e-10 || gate.high > limits.second + 1e-10)
          throw std::invalid_argument("Every gate must stay inside the visible zoom range (including relative instances).");
        if(gates == &config.gates && gate.enabled) ResolveGate(gate, edges, config.flow);
      }
    }
    if(!std::isfinite(config.background.fraction) || config.background.fraction < 0 || config.background.fraction > 1)
      throw std::invalid_argument("Manual fraction requires 0 <= f <= 1.");
    fConfig = config;
    Changed();
    return true;
  } catch(const std::exception& error) {
    SetStatus(error.what());
    return false;
  }
}

// ============== Refresh ==============
// Purpose: Recompute atomically, retaining stale data on failure. Inputs: Live source/settings. Outputs: Calculation status.
bool Session::Refresh(bool resetView) {
  if(fClosed || !fSource) return false;
  if(!resetView) SyncView(false);
  fPending = false;
  auto result = Compute(*fSource, fConfig);
  if(!result.success) {
    fCurrent = false;
    SetStatus("OUT OF DATE: " + result.message);
    RepaintStatus();
    if(resetView) Render(true);
    return false;
  }
  fResult = std::move(result);
  fCurrent = true;
  bool zero = !fResult.projections.empty() && fConfig.background.model == SubtractionModel::Template &&
    fConfig.background.selection != BackgroundTemplate::None;
  for(const auto& projection : fResult.projections)
    for(const auto& component : projection.components) zero = zero && component.alpha == 0.0;
  SetStatus(zero ? "CURRENT: Background scale is zero; Scaled BG = 0, Net = Raw. Check Fraction / Manual f." :
    "CURRENT: Result matches the current settings. " + fResult.message);
  Render(resetView);
  return true;
}

// ============== Tick ==============
// Purpose: Run calculations only after a completed gesture or edit pause.
// Inputs: Main-thread timer tick. Outputs: Safe update or closed invalid canvas session.
void Session::Tick() {
  if(fClosed) return;
  if(fWindow && (!Alive(TotalCanvas()) || !Alive(ResultCanvas()))) { Close(); return; }
  if(fWindow) fWindow->CollectRows();
  SyncView();
  if(fPending && !fGesture && static_cast<unsigned long long>(gSystem->Now()) >= fDue) {
    fPending = false;
    if(fWindow) fWindow->Submit(true);
    else Refresh();
  }
}

// ============== Close ==============
// Purpose: Stop callbacks without touching a deleted matrix. Inputs: Deletion flag. Outputs: Closed/unmapped session.
void Session::Close(bool sourceDeleted) {
  if(fClosed) return;
  fClosed = true;
  fPending = false;
  if(sourceDeleted) fSource = nullptr;
  if(Alive(TotalCanvas())) TotalCanvas()->Disconnect(nullptr, &RootController::Instance());
  if(fWindow) fWindow->Detach();
}

// ============== Raise ==============
// Purpose: Raise an existing analysis. Inputs: None. Outputs: Visible window.
void Session::Raise() { if(fWindow && !fClosed) fWindow->Show(); }

// ============== Select ==============
// Purpose: Change selection without invalidating numerical results. Inputs: Kind/ID. Outputs: Highlight and editor.
void Session::Select(bool background, std::uint64_t id) {
  fSelectedBackground = background;
  fSelectedId = id;
  fFirstBound = false;
  if(fWindow) fWindow->Refresh(true);
  Preview();
}

// ============== AddGate ==============
// Purpose: Add a stable signal/background window on the active grid. Inputs: Kind/optional center. Outputs: Selected gate.
void Session::AddGate(bool background, double center, bool explicitCenter) {
  if(fClosed) return;
  const auto edges = GateEdges();
  if(edges.size() < 2) return;
  SyncView();
  if(background && fConfig.background.model == SubtractionModel::RadWareStandard) {
    SetStatus("Manual BG gates require Model = Template subtraction."); return;
  }
  if(background && fConfig.background.selection != BackgroundTemplate::ManualRelative)
    fConfig.background.selection = BackgroundTemplate::Manual;
  const auto range = GateLimits(background);
  const double span = range.second - range.first;
  const double width = std::min(span, std::max(span / 20.0, edges[1] - edges[0]));
  const double anchor = background && fConfig.background.selection == BackgroundTemplate::ManualRelative ? Anchor() : 0.0;
  if(!explicitCenter) center = (range.first + range.second) / 2.0 + anchor;
  center = std::clamp(center - anchor, range.first + width / 2.0, range.second - width / 2.0);
  auto used = [&](std::uint64_t id) {
    if(!id) return true;
    for(const auto* list : {&fConfig.gates, &fConfig.background.gates})
      for(const auto& gate : *list) if(gate.id == id) return true;
    return false;
  };
  while(used(fNextId)) ++fNextId;
  const auto id = fNextId++;
  double low = center - width / 2.0;
  double high = center + width / 2.0;

  auto& gates = background ? fConfig.background.gates : fConfig.gates;
  gates.push_back({id, background ? "Background" : "Signal", low, high, true});
  const auto visible = VisibleRange();
  ConstrainGates(fConfig, visible, visible);
  Changed();
  Select(background, id);
  Render();
}

// ============== DeleteSelected ==============
// Purpose: Purge one gate and its historical products. Inputs: None. Outputs: Recomputed or empty affected results.
void Session::DeleteSelected() {
  const auto deleted = fSelectedId;
  const bool background = fSelectedBackground;
  auto erase = [&](Config& config) {
    auto& list = background ? config.background.gates : config.gates;
    list.erase(std::remove_if(list.begin(), list.end(), [&](const Gate& gate) { return gate.id == deleted; }), list.end());
    if(background && list.empty() && (config.background.selection == BackgroundTemplate::Manual ||
      config.background.selection == BackgroundTemplate::ManualRelative)) config.background.selection = BackgroundTemplate::None;
  };
  auto& gates = background ? fConfig.background.gates : fConfig.gates;
  if(!SelectedGate()) return;
  erase(fConfig);
  const auto range = VisibleRange();
  ConstrainGates(fConfig, range, range);
  erase(fAxisConfigs[fConfig.gateAxis == Axis::X ? 0 : 1]);
  if(fWindow) fWindow->ForgetGate(fConfig.gateAxis, background, deleted);
  fSelectedId = gates.empty() ? 0 : gates.front().id;
  auto total = std::move(fResult.total);
  fResult = Result{};
  fResult.total = std::move(total);
  fResult.config = fConfig;
  fResultIndex = 0;
  Changed();
  if(fWindow) fWindow->Refresh(true);
  Refresh();
  Render();
}

// ============== SetBounds ==============
// Purpose: Move selected boundaries without changing identity. Inputs: Coordinates/preview flag. Outputs: Accepted bounds.
bool Session::SetBounds(double low, double high, bool preview) {
  auto* gate = SelectedGate();
  if(!gate || !std::isfinite(low) || !std::isfinite(high) || low >= high) return false;
  const auto limits = GateLimits(fSelectedBackground);
  if(low < limits.first || high > limits.second) {
    SetStatus("Gate bounds must stay inside the visible zoom range."); return false;
  }
  gate->low = low;
  gate->high = high;
  const auto range = VisibleRange();
  ConstrainGates(fConfig, range, range);
  Changed();
  if(fWindow) fWindow->Refresh(true);
  if(preview) Preview(); else Render();
  return true;
}

// ============== SwitchAxis ==============
// Purpose: Keep independent axis configurations without reinterpreting energy/time coordinates.
// Inputs: New axis. Outputs: Recomputed total and restored per-axis gates.
void Session::SwitchAxis(Axis axis) {
  if(axis == fConfig.gateAxis || fClosed) return;
  fAxisConfigs[fConfig.gateAxis == Axis::X ? 0 : 1] = fConfig;
  fConfig = fAxisConfigs[axis == Axis::X ? 0 : 1];
  fSelectedId = fConfig.gates.empty() ? 0 : fConfig.gates.front().id;
  fSelectedBackground = false;
  fResultIndex = 0;
  fFirstBound = false;
  Changed();
  Refresh(true);
  if(fWindow) fWindow->Refresh(true);
}

// ============== SetAuto ==============
// Purpose: Control deferred recomputation. Inputs: Enabled flag. Outputs: Updated scheduler.
void Session::SetAuto(bool enabled) {
  fAuto = enabled;
  fPending = enabled && !fCurrent;
  fDue = static_cast<unsigned long long>(gSystem->Now()) + 250;
  if(fWindow) fWindow->Refresh();
}

// ============== SetBrowse ==============
// Purpose: Pause custom picking while keeping native ROOT input active.
// Inputs: Browse flag. Outputs: Cleared gesture and active tool.
void Session::SetBrowse(bool browse) {
  fBrowse = browse;
  fFirstBound = false;
  fDragging = -1;
  fGesture = false;
  SetStatus(browse ? "Browse: native ROOT canvas interaction." : "Pick range: click two edges; drag a boundary; double-click a center.");
}

// ============== SetView ==============
// Purpose: Select independent display layers. Inputs: Layer flags. Outputs: Redrawn last result.
void Session::SetView(bool raw, bool background, bool net) {
  fShowRaw = raw; fShowBackground = background; fShowNet = net || (!raw && !background);
  Render();
}

// ============== SetResultIndex ==============
// Purpose: Browse Separate results without invalidating the calculation. Inputs: Index. Outputs: Selected spectrum.
void Session::SetResultIndex(std::size_t index) { fResultIndex = index; Render(); }

// ============== SetGesture ==============
// Purpose: Suspend updates while native widgets hold a drag. Inputs: Active flag. Outputs: Gesture guard.
void Session::SetGesture(bool active) { fGesture = active; if(!active) fDue = static_cast<unsigned long long>(gSystem->Now()) + 150; }

// ============== ClearDrawings ==============
// Purpose: Detach primitives before deleting owned display copies. Inputs: Owned drawings. Outputs: Empty pads.
void Session::ClearDrawings() {
  if(Alive(TotalCanvas())) TotalCanvas()->Clear();
  if(Alive(ResultCanvas())) ResultCanvas()->Clear();
  fMarkers.clear(); fLegend.reset(); fLeftStatus.reset(); fRightStatus.reset();
  fTotalDisplay.reset(); fEstimateDisplay.reset();
  for(auto& display : fDisplays) display.reset();
}

// ============== Render ==============
// Purpose: Draw snapshots through isolated copies and preserve native zoom.
// Inputs: Optional view reset. Outputs: Updated result and gate canvases.
void Session::Render(bool resetView) {
  if(fClosed || !Alive(TotalCanvas()) || !Alive(ResultCanvas())) return;
  if(!resetView) SyncView(false);
  const auto range = VisibleRange();
  const View gateView{true, range.first, range.second};
  View resultView;
  if(!resetView) for(const auto& display : fDisplays) if(display) { resultView = ReadView(display.get()); break; }
  ClearDrawings();
  TVirtualPad::TContext context(TotalCanvas());
  if(!fResult.total) return;
  fTotalDisplay = Copy(*fResult.total);
  RestoreView(*fTotalDisplay, gateView);
  fTotalDisplay->SetTitle((std::string("Gate axis: ") + ToString(fResult.config.gateAxis) + " - Total projection").c_str());
  fTotalDisplay->Draw("hist");
  if(fResult.estimatedBackground) {
    fEstimateDisplay = Copy(*fResult.estimatedBackground);
    fEstimateDisplay->SetLineColor(kGreen + 2); fEstimateDisplay->SetLineStyle(7); fEstimateDisplay->SetLineWidth(2);
    fEstimateDisplay->Draw("hist same");
  }
  TotalCanvas()->Modified(); TotalCanvas()->Update();
  DrawMarkers();
  ResultCanvas()->cd();
  if(!fResult.projections.empty()) {
    fResultIndex = std::min(fResultIndex, fResult.projections.size() - 1);
    const auto& result = fResult.projections[fResultIndex];
    const TH1D* sources[] = {result.raw.get(), result.subtracted.get(), result.net.get()};
    const bool shown[] = {fShowRaw, fShowBackground, fShowNet};
    const char* names[] = {"Raw gated", "Subtracted background", "Net gated"};
    const int colors[] = {kBlack, kGreen + 2, kBlue + 1};
    double maximum = 0.0;
    double minimum = 0.0;
    for(int layer = 0; layer < 3; ++layer) if(shown[layer] && sources[layer]) {
      fDisplays[layer] = Copy(*sources[layer]);
      RestoreView(*fDisplays[layer], resultView);
      maximum = std::max(maximum, fDisplays[layer]->GetMaximum());
      minimum = std::min(minimum, fDisplays[layer]->GetMinimum());
    }
    fLegend = std::make_unique<TLegend>(0.64, 0.74, 0.91, 0.89);
    fLegend->SetFillStyle(0); fLegend->SetBorderSize(0);
    fLegend->SetTextSize(0.035);
    bool first = true;
    for(int layer = 0; layer < 3; ++layer) if(fDisplays[layer]) {
      auto& histogram = *fDisplays[layer];
      histogram.SetTitle("Gated result");
      histogram.SetLineColor(colors[layer]); histogram.SetLineWidth(2);
      if(layer == 1) histogram.SetLineStyle(7);
      if(!ResultCanvas()->GetLogy()) {
        histogram.SetMaximum(maximum > 0.0 ? maximum * 1.15 : 1.0);
        histogram.SetMinimum(minimum < 0.0 ? minimum * 1.15 : 0.0);
      }
      histogram.Draw(first ? "hist" : "hist same");
      first = false;
      fLegend->AddEntry(&histogram, names[layer], "l");
    }
    fLegend->Draw();
  } else {
    ResultCanvas()->DrawFrame(0, 0, 1, 1, "No signal gate;Result axis;Counts");
  }
  RepaintStatus();
  ResultCanvas()->Modified(); ResultCanvas()->Update();
  if(fWindow) fWindow->Refresh();
}

// ============== RepaintStatus ==============
// Purpose: Mark historical spectra visibly without recreating dragged primitives.
// Inputs: Current/stale flag. Outputs: Non-pickable status stamps on both pads.
void Session::RepaintStatus() {
  if(!fWindow || !Alive(TotalCanvas()) || !Alive(ResultCanvas())) return;
  TVirtualPad::TContext context(TotalCanvas());
  if(fRightStatus) TotalCanvas()->GetListOfPrimitives()->Remove(fRightStatus.get());
  if(fLeftStatus) ResultCanvas()->GetListOfPrimitives()->Remove(fLeftStatus.get());
  fRightStatus.reset(); fLeftStatus.reset();
  if(!fCurrent) {
    TotalCanvas()->cd(); fRightStatus = Stamp("OUT OF DATE"); fRightStatus->Draw();
    ResultCanvas()->cd(); fLeftStatus = Stamp("OUT OF DATE"); fLeftStatus->Draw();
  } else if(fResult.projections.empty()) {
    ResultCanvas()->cd(); fLeftStatus = Stamp("Pick two edges on the total projection", 0.45); fLeftStatus->Draw();
  }
  TotalCanvas()->Modified(); ResultCanvas()->Modified();
}

// ============== DrawMarkers ==============
// Purpose: Draw stable gate boundaries on the current total view. Inputs: Configuration. Outputs: Owned vertical lines.
void Session::DrawMarkers() {
  if(!Alive(TotalCanvas()) || !fTotalDisplay || fConfig.gateAxis != fResult.config.gateAxis) return;
  TotalCanvas()->cd();
  std::vector<double> anchors{0.0};
  if(fConfig.background.selection == BackgroundTemplate::ManualRelative) {
    anchors = {Anchor()};
    if(fConfig.combination == Combination::Sum) {
      anchors.clear();
      for(const auto& gate : fConfig.gates) if(gate.enabled) anchors.push_back((gate.low + gate.high) / 2.0);
    }
  }
  auto add = [&](const Gate& gate, bool background, double anchor) {
    if(!gate.enabled) return;
    for(int edge = 0; edge < 2; ++edge) {
      Marker marker;
      marker.id = gate.id; marker.background = background; marker.upper = edge; marker.anchor = anchor;
      if(background && fConfig.background.selection == BackgroundTemplate::ManualRelative && fConfig.combination == Combination::Sum)
        for(const auto& signal : fConfig.gates)
          if(signal.enabled && (signal.low + signal.high) / 2.0 == anchor) { marker.anchorId = signal.id; break; }
      const double x = (edge ? gate.high : gate.low) + anchor;
      marker.line = std::make_unique<TLine>(x, TotalCanvas()->PadtoY(TotalCanvas()->GetUymin()),
        x, TotalCanvas()->PadtoY(TotalCanvas()->GetUymax()));
      marker.line->SetVertical();
      marker.line->SetUniqueID(kGateMarker);
      marker.line->SetBit(TObject::kMustCleanup);
      marker.line->SetLineColor(GateColor(gate.id));
      marker.line->SetLineStyle(background ? 7 : 2);
      marker.line->SetLineWidth(gate.id == fSelectedId && background == fSelectedBackground ? 3 : 2);
      marker.line->Draw();
      fMarkers.push_back(std::move(marker));
    }
  };
  for(const auto& gate : fConfig.gates) add(gate, false, 0);
  for(const auto& gate : fConfig.background.gates) for(const double anchor : anchors) add(gate, true, anchor);
  TotalCanvas()->Modified(); TotalCanvas()->Update();
}

// ============== Preview ==============
// Purpose: Reposition existing boundaries safely during a native drag. Inputs: Settings. Outputs: Marker coordinates.
void Session::Preview() {
  if(!Alive(TotalCanvas())) return;
  TVirtualPad::TContext context(TotalCanvas());
  for(auto& marker : fMarkers) {
    const auto& gates = marker.background ? fConfig.background.gates : fConfig.gates;
    if(marker.background && fConfig.background.selection == BackgroundTemplate::ManualRelative) {
      marker.anchor = Anchor();
      if(marker.anchorId) for(const auto& signal : fConfig.gates)
        if(signal.id == marker.anchorId) marker.anchor = (signal.low + signal.high) / 2.0;
    }
    for(const auto& gate : gates) if(gate.id == marker.id) {
      const double x = (marker.upper ? gate.high : gate.low) + marker.anchor;
      marker.line->SetX1(x); marker.line->SetX2(x);
      marker.line->SetY1(TotalCanvas()->PadtoY(TotalCanvas()->GetUymin()));
      marker.line->SetY2(TotalCanvas()->PadtoY(TotalCanvas()->GetUymax()));
      marker.line->SetLineWidth(gate.id == fSelectedId && marker.background == fSelectedBackground ? 3 : 2);
    }
  }
  TotalCanvas()->Modified(); TotalCanvas()->Update();
}

// ============== CanvasEvent ==============
// Purpose: Apply gate gestures after ROOT input, preserving native drags on other objects.
// Inputs: Canvas-local event and selected object. Outputs: Stable gate edits or Browse behavior.
void Session::CanvasEvent(TCanvas* canvas, int event, int px, int py, TObject* selected) {
  if(fClosed || canvas != TotalCanvas()) return;
  if(event == kKeyPress) {
    const int key = py ? py : px;
    if(key == kKey_Escape) { SetBrowse(true); return; }
    if(fBrowse) return;
    if(key == kKey_Delete) DeleteSelected();
    if((key == kKey_Return || key == kKey_Enter) && fWindow) fWindow->Submit(true);
    return;
  }
  if(!fGesture) SyncView();
  if(fBrowse || !fTotalDisplay) return;
  const int left = canvas->XtoAbsPixel(canvas->GetUxmin());
  const int right = canvas->XtoAbsPixel(canvas->GetUxmax());
  const int top = canvas->YtoAbsPixel(canvas->GetUymax());
  const int bottom = canvas->YtoAbsPixel(canvas->GetUymin());
  const bool inPlot = px >= left && px <= right && py >= top && py <= bottom;
  const double x = canvas->PadtoX(canvas->AbsPixeltoX(px));
  const bool allowed = selected == fTotalDisplay.get() || dynamic_cast<TFrame*>(selected) ||
    (selected && selected->GetUniqueID() == kGateMarker);
  if(event == kButton1Double && inPlot && allowed) {
    if(fWindow && !fWindow->Submit(false)) return;
    fFirstBound = false; fIgnoreUp = true;
    auto* gate = SelectedGate();
    if(!gate) AddGate(false, x, true);
    else {
      const double offset = fSelectedBackground && fConfig.background.selection == BackgroundTemplate::ManualRelative ? Anchor() : 0.0;
      const double width = gate->high - gate->low;
      const auto limits = GateLimits(fSelectedBackground);
      const double low = std::clamp(x - offset - width / 2.0, limits.first, limits.second - width);
      SetBounds(low, low + width);
    }
    return;
  }
  if(event == kButton1Down && inPlot && allowed) {
    if(fWindow && !fWindow->Submit(false)) return;
    fClick = true; fDownX = px; fDownY = py; fDragging = -1;
    for(std::size_t index = 0; index < fMarkers.size(); ++index) {
      auto& marker = fMarkers[index];
      if(selected == marker.line.get()) {
        Select(marker.background, marker.id);
        fDragging = static_cast<int>(index);
        SetGesture(true);
        break;
      }
    }
  } else if(event == kButton1Motion && fDragging >= 0) {
    auto& marker = fMarkers[fDragging];
    const auto limits = GateLimits(marker.background);
    const double point = std::clamp(x - marker.anchor, limits.first, limits.second);
    auto* gate = SelectedGate();
    if(!gate) return;
    double low = marker.upper ? gate->low : point;
    double high = marker.upper ? point : gate->high;
    if(low > high) {
      std::swap(low, high);
      for(auto& other : fMarkers)
        if(other.id == marker.id && other.background == marker.background) other.upper = !other.upper;
    }
    if(low < high) SetBounds(low, high, true);
  } else if(event == kButton1Up) {
    if(fIgnoreUp) { fIgnoreUp = false; fClick = false; return; }
    if(fDragging >= 0) {
      fDragging = -1; fClick = false; SetGesture(false); Render(); return;
    }
    if(fClick && inPlot && std::abs(px - fDownX) <= 4 && std::abs(py - fDownY) <= 4) {
      if(!fFirstBound) { fFirstX = x; fFirstBound = true; SetStatus("First edge selected. Click the second edge."); }
      else {
        fFirstBound = false;
        const double low = std::min(x, fFirstX);
        const double high = std::max(x, fFirstX);
        if(low < high) {
          if(!SelectedGate()) AddGate(false, (low + high) / 2.0, true);
          const double anchor = fSelectedBackground && fConfig.background.selection == BackgroundTemplate::ManualRelative ? Anchor() : 0.0;
          SetBounds(low - anchor, high - anchor);
        }
      }
    }
    fClick = false;
  }
}

} // namespace MatrixGate::Detail

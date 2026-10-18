#include "MatrixGateWindow.h"
#include "MatrixGateSession.h"
#include "MatrixGateProjection.h"
#include "MatrixGateReportView.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>
#include <stdexcept>
#include <TGButton.h>
#include <TGComboBox.h>
#include <TGDoubleSlider.h>
#include <TGLabel.h>
#include <TGCanvas.h>
#include <TColor.h>
#include <TSystem.h>
#include <TROOT.h>
#include <TGTab.h>
#include <TGTextEntry.h>
#include <TGClient.h>
#include <TH2.h>
#include <TCanvas.h>
#include <TRootEmbeddedCanvas.h>
#include <TString.h>

namespace MatrixGate::Detail {
namespace {

class Entry : public TGTextEntry {
public:
  // ============== Entry ==============
  // Purpose: Retain numeric text until explicit validation. Inputs: Parent/ID/receiver. Outputs: Entry.
  Entry(const TGWindow* parent, int id, TGWindow* receiver) : TGTextEntry(parent, "", id) {
    Associate(receiver); SetDefaultSize(82, 24);
  }
  // ============== HandleFocusChange ==============
  // Purpose: Commit complete edits on focus loss. Inputs: Native focus event. Outputs: Validation request.
  Bool_t HandleFocusChange(Event_t* event) override {
    const auto handled = TGTextEntry::HandleFocusChange(event);
    if(event->fType == kFocusOut) SendMessage(fMsgWindow, MK_MSG(kC_TEXTENTRY, kTE_ENTER), WidgetId(), 0);
    return handled;
  }
};

// ============== Number ==============
// Purpose: Read finite numeric drafts without ROOT's automatic coercion.
// Inputs: Entry and label. Outputs: Parsed number or exception.
double Number(TGTextEntry* entry, const char* label) {
  const char* begin = entry->GetText();
  char* end = nullptr;
  errno = 0;
  const double value = std::strtod(begin, &end);
  const bool empty = begin == end;
  if(end) while(std::isspace(static_cast<unsigned char>(*end))) ++end;
  if(empty || !end || *end || errno == ERANGE || !std::isfinite(value))
    throw std::invalid_argument(std::string(label) + ": enter a finite number.");
  return value;
}

// ============== Integer ==============
// Purpose: Reject fractional or overflowing iteration settings. Inputs: Entry/label. Outputs: Integer.
int Integer(TGTextEntry* entry, const char* label) {
  const double value = Number(entry, label);
  if(value < 0 || value > 1000000 || value != std::floor(value))
    throw std::invalid_argument(std::string(label) + ": enter a nonnegative integer.");
  return static_cast<int>(value);
}

// ============== SetNumber ==============
// Purpose: Render numbers without generating edit notifications. Inputs: Entry/value. Outputs: Text.
void SetNumber(TGTextEntry* entry, double value) { entry->SetText(TString::Format("%.12g", value), false); }

// ============== DestroyTree ==============
// Purpose: Destroy native Cocoa controls before releasing detached rows. Inputs: Frame tree. Outputs: Removed windows.
void DestroyTree(TGFrame* frame) {
  if(auto* composite = dynamic_cast<TGCompositeFrame*>(frame)) {
    TIter children(composite->GetList());
    while(auto* item = static_cast<TGFrameElement*>(children())) DestroyTree(item->fFrame);
  }
  frame->DestroyWindow();
}

} // namespace

class Window::Impl {
public:
  Session* session = nullptr;
  TRootEmbeddedCanvas* total = nullptr;
  TRootEmbeddedCanvas* result = nullptr;
  struct GateRow {
    TGVerticalFrame* frame = nullptr;
    TGTextButton* select = nullptr;
    TGCheckButton* enabled = nullptr;
    TGDoubleHSlider* slider = nullptr;
    bool background = false;
    Axis axis = Axis::X;
    std::uint64_t id = 0;
    int selectId = 0, enabledId = 0, sliderId = 0, deleteId = 0;
  };
  struct RetiredRow {
    std::unique_ptr<TGVerticalFrame> frame;
    unsigned long long due = 0;
  };
  TGCanvas* gateViewport = nullptr;
  TGVerticalFrame* gateRows = nullptr;
  std::vector<GateRow> rows;
  std::vector<RetiredRow> retiredRows;
  int nextWidgetId = 1000;
  TGLabel* target = nullptr;
  TGLabel* status = nullptr;
  TGLabel* actual = nullptr;
  TGLabel* curve = nullptr;
  MatrixGateReportView* report = nullptr;
  std::map<int, TGTextEntry*> entries;
  std::map<int, TGComboBox*> combos;
  std::map<int, TGCheckButton*> checks;
  std::map<int, TGTextButton*> buttons;
  std::array<std::array<BackgroundConfig, 3>, 2> modelCache;
  std::array<std::array<std::pair<double, double>, 3>, 2> modelViews{};
  std::array<std::array<ErrorPolicy, 3>, 2> errorCache{};
  std::uint64_t editedId = 0;
  int boundsEditor = 0;
  std::string reportText;
  bool editedBackground = false;
  bool dirty = false;
  bool refreshing = false;
  bool submitting = false;
  bool sliderBlocked = false;
  bool shown = false;
};

// ============== Window ==============
// Purpose: Build an independent native ROOT analysis window.
// Inputs: Owning session. Outputs: Two canvases, gate editors, background controls, and report viewport.
Window::Window(Session* session) : TGMainFrame(gClient->GetRoot(), 1320, 870), fImpl(std::make_unique<Impl>()) {
  fImpl->session = session;
  SetCleanup(kDeepCleanup);
  SetWindowName((std::string("MatrixGate - ") + session->Source()->GetName()).c_str());
  for(int axis = 0; axis < 2; ++axis) for(int model = 0; model < 3; ++model)
    fImpl->modelCache[axis][model].model = static_cast<SubtractionModel>(model);
  for(int axis = 0; axis < 2; ++axis) {
    const auto* grid = axis == 0 ? session->Source()->GetXaxis() : session->Source()->GetYaxis();
    for(auto& view : fImpl->modelViews[axis]) view = {grid->GetXmin(), grid->GetXmax()};
  }
  const auto& initial = session->Configuration();
  const int axisIndex = initial.gateAxis == Axis::X ? 0 : 1;
  fImpl->modelCache[axisIndex][static_cast<int>(initial.background.model)] = initial.background;
  fImpl->errorCache[axisIndex][static_cast<int>(initial.background.model)] = initial.errors;
  auto row = [&](TGCompositeFrame* parent) {
    auto* output = new TGHorizontalFrame(parent);
    parent->AddFrame(output, new TGLayoutHints(kLHintsExpandX, 4, 4, 2, 2));
    return output;
  };
  auto label = [&](TGCompositeFrame* parent, const char* text) {
    auto* output = new TGLabel(parent, text);
    parent->AddFrame(output, new TGLayoutHints(kLHintsCenterY, 5, 4, 2, 2));
    return output;
  };
  auto button = [&](TGCompositeFrame* parent, const char* text, int id) {
    auto* output = new TGTextButton(parent, text, id);
    output->Associate(this);
    parent->AddFrame(output, new TGLayoutHints(kLHintsLeft, 4, 4, 2, 2));
    fImpl->buttons[id] = output;
    return output;
  };
  auto entry = [&](TGCompositeFrame* parent, const char* text, int id, int width = 82) {
    label(parent, text);
    auto* output = new Entry(parent, id, this);
    output->SetDefaultSize(width, 24);
    parent->AddFrame(output, new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 2, 8, 2, 2));
    fImpl->entries[id] = output;
    return output;
  };
  auto combo = [&](TGCompositeFrame* parent, const char* text, int id,
    const std::vector<std::pair<int, const char*>>& choices, int width) {
    label(parent, text);
    auto* output = new TGComboBox(parent, id);
    for(const auto& choice : choices) output->AddEntry(choice.second, choice.first);
    output->Associate(this); output->Resize(width, 24);
    parent->AddFrame(output, new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 2, 8, 2, 2));
    fImpl->combos[id] = output;
    return output;
  };
  auto check = [&](TGCompositeFrame* parent, const char* text, int id) {
    auto* output = new TGCheckButton(parent, text, id);
    output->Associate(this);
    parent->AddFrame(output, new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 4, 8, 2, 2));
    fImpl->checks[id] = output;
    return output;
  };
  fImpl->target = new TGLabel(this, (std::string("Source: ") + session->Source()->GetName() +
    " | Left: gated result | Right: gate-axis total projection").c_str());
  AddFrame(fImpl->target, new TGLayoutHints(kLHintsExpandX, 10, 10, 6, 2));
  auto* toolbar = row(this);
  combo(toolbar, "Gate axis", kAxis, {{0, "X -> Y"}, {1, "Y -> X"}}, 95);
  combo(toolbar, "Gates", kCombination, {{0, "Separate"}, {1, "Sum"}, {2, "Union"}}, 100);
  button(toolbar, "Pick range", kPick); button(toolbar, "Browse", kBrowse);
  button(toolbar, "Update", kUpdate); check(toolbar, "Auto Update", kAuto);
  check(toolbar, "Raw", kRaw); check(toolbar, "Scaled BG", kSubtracted); check(toolbar, "Net", kNet);
  fImpl->checks.at(kNet)->SetOn(true, false);
  button(toolbar, "Close", kExit);
  fImpl->checks.at(kSubtracted)->SetToolTipText("Correction alpha * H actually subtracted from Raw. Net = Raw - Scaled BG; BG windows are combined as a union.");
  fImpl->status = new TGLabel(this, "Ready");
  fImpl->status->SetTextJustify(kTextLeft); fImpl->status->SetWrapLength(1250);
  AddFrame(fImpl->status, new TGLayoutHints(kLHintsExpandX, 10, 10, 2, 3));
  auto* canvases = new TGHorizontalFrame(this, 1300, 410, kFixedHeight);
  fImpl->result = new TRootEmbeddedCanvas("MatrixGateResult", canvases, 640, 410);
  fImpl->total = new TRootEmbeddedCanvas("MatrixGateTotal", canvases, 640, 410);
  canvases->AddFrame(fImpl->result, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 2, 2, 2, 2));
  canvases->AddFrame(fImpl->total, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 2, 2, 2, 2));
  AddFrame(canvases, new TGLayoutHints(kLHintsExpandX, 4, 4, 0, 0));
  auto* resultRow = row(this);
  combo(resultRow, "Shown result", kResult, {}, 460);
  label(resultRow, "Gate colors match rows | Pick mode: double-click RIGHT plot to move selected center (same width)");
  auto* tabs = new TGTab(this, 1280, 260);
  AddFrame(tabs, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 6, 6, 2, 2));
  auto* gates = tabs->AddTab("Gates");
  auto* actions = row(gates);
  button(actions, "+ Signal", kAddSignal); button(actions, "+ Background", kAddBackground);
  button(actions, "Delete selected", kDelete);
  label(actions, "Pick two edges edits the selected gate; with no selection it creates a signal gate.");
  auto* editors = row(gates);
  fImpl->gateViewport = new TGCanvas(editors, 590, 180);
  fImpl->gateRows = new TGVerticalFrame(fImpl->gateViewport->GetViewPort(), 550, 180);
  fImpl->gateRows->SetCleanup(kDeepCleanup);
  fImpl->gateViewport->SetContainer(fImpl->gateRows);
  fImpl->gateViewport->GetViewPort()->SetCleanup(kDeepCleanup);
  fImpl->gateViewport->SetScrolling(TGCanvas::kCanvasScrollVertical);
  editors->AddFrame(fImpl->gateViewport, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 3, 8, 2, 2));
  auto* settings = new TGVerticalFrame(editors, 680, 180);
  editors->AddFrame(settings, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
  auto* identity = row(settings);
  entry(identity, "Name", kName, 185); check(identity, "Enabled", kEnabled);
  auto* bounds = row(settings);
  entry(bounds, "Low", kLow, 125); entry(bounds, "High", kHigh, 125);
  auto* center = row(settings);
  entry(center, "Center", kCenter, 125); entry(center, "Width", kWidth, 125);
  fImpl->actual = new TGLabel(settings, "No gate selected");
  fImpl->actual->SetWrapLength(600);
  settings->AddFrame(fImpl->actual, new TGLayoutHints(kLHintsExpandX, 6, 6, 2, 2));
  auto* background = tabs->AddTab("Background");
  auto* models = row(background);
  combo(models, "Model", kModel, {{1, "Template subtraction"}, {2, "RadWare standard 2D"}}, 185);
  combo(models, "Template", kTemplate, {{0, "None"}, {1, "Full"}, {2, "Compton"}, {3, "Anti"}, {4, "Manual"}, {5, "Manual Relative"}}, 135);
  combo(models, "Fraction", kFractionMethod, {{0, "Linear Fixed"}, {1, "Polynomial Flex"}, {2, "Manual"}, {3, "TSpectrum"}, {4, "Width ratio"}}, 140);
  combo(models, "Errors", kErrors, {{0, "Fixed coefficients"}, {1, "jROOT compatible"}, {2, "RadWare compatible"}}, 160);
  auto* numbers = row(background);
  entry(numbers, "Manual f", kFraction); entry(numbers, "Relative error (-1=preset)", kUncertainty);
  entry(numbers, "Iterations", kIterations, 65); entry(numbers, "Compton gap", kComptonGap, 65);
  check(numbers, "Symmetric gamma-gamma (RadWare)", kSymmetric);
  auto* conventions = row(background);
  entry(conventions, "TSpectrum options", kOptions, 215);
  combo(conventions, "Fraction integral", kIntegral, {{0, "Counts"}, {1, "Width weighted"}}, 125);
  combo(conventions, "Manual overlap", kOverlap, {{0, "Reference"}, {1, "Exclude signal"}, {2, "Allow overlap"}}, 130);
  button(conventions, "Use automatic curve", kAutomaticCurve);
  auto* ranges = row(background);
  check(ranges, "Fit range", kUseFitRange); entry(ranges, "Low", kFitLow); entry(ranges, "High", kFitHigh);
  check(ranges, "Normalization range", kUseNormRange); entry(ranges, "Low", kNormLow); entry(ranges, "High", kNormHigh);
  auto* flows = row(background);
  check(flows, "Gate underflow", kGateUnder); check(flows, "Gate overflow", kGateOver);
  check(flows, "Result underflow", kResultUnder); check(flows, "Result overflow", kResultOver);
  fImpl->curve = label(flows, "Automatic projection background");
  auto* report = tabs->AddTab("Report");
  fImpl->report = new MatrixGateReportView(report, 1250, 220);
  report->AddFrame(fImpl->report, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 3, 3, 3, 3));
  fImpl->entries.at(kFraction)->SetToolTipText("Fraction of Raw integral assigned to background (0..1). f=0 intentionally gives Net=Raw. Used only by Fraction=Manual.");
  fImpl->entries.at(kIterations)->SetToolTipText("TSpectrum clipping iterations. Signed bins are passed unchanged in Template mode.");
  fImpl->entries.at(kLow)->SetToolTipText("Physical lower edge; Manual Relative uses offsets from the signal center. All physical instances must remain inside the zoom.");
  fImpl->entries.at(kHigh)->SetToolTipText("Exclusive upper edge. Slider bounds follow the visible right-hand X axis.");
  fImpl->buttons.at(kPick)->SetToolTipText("On the RIGHT total plot: two clicks set edges; double-click moves the selected gate center without changing width; drag a boundary to resize.");
  Refresh(true);
}

// ============== ~Window ==============
// Purpose: Release native child widgets through ROOT cleanup. Inputs: Owned frame. Outputs: Closed window.
Window::~Window() { CollectRows(true); }

// ============== CollectRows ==============
// Purpose: Release detached controls after their callbacks finish. Inputs: Drain flag. Outputs: No orphan native rows.
void Window::CollectRows(bool all) {
  const auto now = static_cast<unsigned long long>(gSystem->Now());
  auto& rows = fImpl->retiredRows;
  rows.erase(std::remove_if(rows.begin(), rows.end(), [&](auto& row) {
    if(!all && row.due > now) return false;
    DestroyTree(row.frame.get()); return true;
  }), rows.end());
}

// ============== ForgetGate ==============
// Purpose: Purge a deleted identity from inactive model caches. Inputs: Axis/kind/ID. Outputs: No resurrection on model switch.
void Window::ForgetGate(Axis axis, bool background, std::uint64_t id) {
  for(auto& config : fImpl->modelCache[axis == Axis::X ? 0 : 1]) {
    if(!background) continue;
    auto& list = config.gates;
    list.erase(std::remove_if(list.begin(), list.end(), [&](const Gate& gate) { return gate.id == id; }), list.end());
    if(list.empty() && (config.selection == BackgroundTemplate::Manual || config.selection == BackgroundTemplate::ManualRelative))
      config.selection = BackgroundTemplate::None;
  }
  fImpl->dirty = false;
  fImpl->boundsEditor = 0;
}

// ============== Show ==============
// Purpose: Map or raise the existing window. Inputs: None. Outputs: Visible independent window.
void Window::Show() {
  MapSubwindows();
  if(!fImpl->shown) { Resize(1320, 870); fImpl->shown = true; }
  MapWindow(); RaiseWindow();
}

// ============== Detach ==============
// Purpose: Ignore queued callbacks before deferred deletion. Inputs: None. Outputs: Hidden detached window.
void Window::Detach() { fImpl->session = nullptr; UnmapWindow(); }

// ============== CloseWindow ==============
// Purpose: Discard drafts and request safe session shutdown. Inputs: Close control. Outputs: Closed session.
void Window::CloseWindow() { if(fImpl->session) fImpl->session->Close(); }

// ============== Dirty ==============
// Purpose: Read draft validity state. Inputs: None. Outputs: Whether edits remain uncommitted.
bool Window::Dirty() const { return fImpl->dirty; }

// ============== TotalCanvas ==============
// Purpose: Locate the embedded gate canvas. Inputs: None. Outputs: Borrowed canvas.
TCanvas* Window::TotalCanvas() const { return fImpl->total->GetCanvas(); }

// ============== ResultCanvas ==============
// Purpose: Locate the embedded output canvas. Inputs: None. Outputs: Borrowed canvas.
TCanvas* Window::ResultCanvas() const { return fImpl->result->GetCanvas(); }

// ============== Submit ==============
// Purpose: Commit all fields through one validation path without losing invalid drafts.
// Inputs: Recompute flag. Outputs: Successful commit/calculation or visible diagnostic.
bool Window::Submit(bool calculate) {
  auto* session = fImpl->session;
  if(!session || fImpl->submitting) return false;
  session->SyncView();
  fImpl->submitting = true;
  try {
    if(fImpl->dirty) {
      Config candidate = session->Configuration();
      auto e = [&](int id) { return fImpl->entries.at(id); };
      auto c = [&](int id) { return fImpl->combos.at(id)->GetSelected(); };
      auto b = [&](int id) { return fImpl->checks.at(id)->IsOn(); };
      if(fImpl->boundsEditor == kCenter || fImpl->boundsEditor == kWidth) {
        const double center = Number(e(kCenter), "Center");
        const double width = Number(e(kWidth), "Width");
        if(width <= 0.0) throw std::invalid_argument("Width must be positive.");
        SetNumber(e(kLow), center - width / 2.0);
        SetNumber(e(kHigh), center + width / 2.0);
      }
      auto& edited = fImpl->editedBackground ? candidate.background.gates : candidate.gates;
      for(auto& gate : edited) if(gate.id == fImpl->editedId) {
        gate.name = e(kName)->GetText(); gate.enabled = b(kEnabled);
        gate.low = Number(e(kLow), "Gate low"); gate.high = Number(e(kHigh), "Gate high");
      }
      candidate.combination = static_cast<Combination>(c(kCombination));
      auto& bg = candidate.background;
      bg.model = static_cast<SubtractionModel>(c(kModel));
      bg.selection = static_cast<BackgroundTemplate>(c(kTemplate));
      bg.fractionMethod = static_cast<FractionMethod>(c(kFractionMethod));
      if(e(kFraction)->IsEnabled()) bg.fraction = Number(e(kFraction), "Manual fraction");
      if(e(kUncertainty)->IsEnabled()) bg.relativeUncertainty = Number(e(kUncertainty), "Relative uncertainty");
      if(e(kIterations)->IsEnabled()) bg.spectrumIterations = Integer(e(kIterations), "Iterations");
      if(e(kComptonGap)->IsEnabled()) bg.comptonGapBins = Integer(e(kComptonGap), "Compton gap");
      bg.spectrumOptions = e(kOptions)->GetText();
      bg.fractionIntegral = static_cast<FractionIntegral>(c(kIntegral));
      bg.manualOverlap = static_cast<ManualOverlap>(c(kOverlap));
      bg.symmetricGammaGamma = b(kSymmetric);
      bg.useFitRange = b(kUseFitRange);
      if(e(kFitLow)->IsEnabled()) { bg.fitLow = Number(e(kFitLow), "Fit low"); bg.fitHigh = Number(e(kFitHigh), "Fit high"); }
      bg.useNormalizationRange = b(kUseNormRange);
      if(e(kNormLow)->IsEnabled()) {
        bg.normalizationLow = Number(e(kNormLow), "Normalization low"); bg.normalizationHigh = Number(e(kNormHigh), "Normalization high");
      }
      if(bg.fractionMethod == FractionMethod::Width) bg.useNormalizationRange = false;
      candidate.errors = static_cast<ErrorPolicy>(c(kErrors));
      candidate.flow = {b(kGateUnder), b(kGateOver), b(kResultUnder), b(kResultOver)};
      const auto& previous = session->Configuration().background;
      const int axis = candidate.gateAxis == Axis::X ? 0 : 1;
      if(bg.model != previous.model) {
        auto saved = bg; saved.model = previous.model;
        fImpl->modelCache[axis][static_cast<int>(previous.model)] = saved;
        fImpl->errorCache[axis][static_cast<int>(previous.model)] = candidate.errors;
        fImpl->modelViews[axis][static_cast<int>(previous.model)] = session->VisibleRange();
        const int model = static_cast<int>(bg.model);
        bg = fImpl->modelCache[axis][model];
        candidate.errors = fImpl->errorCache[axis][model];
        session->ConstrainGates(candidate, fImpl->modelViews[axis][model], session->VisibleRange());
      } else if(bg.selection != previous.selection) {
        const bool wasRelative = previous.selection == BackgroundTemplate::ManualRelative;
        const bool isRelative = bg.selection == BackgroundTemplate::ManualRelative;
        if(wasRelative != isRelative) for(auto& gate : bg.gates) {
          const double shift = (wasRelative ? 1.0 : -1.0) * session->Anchor();
          gate.low += shift; gate.high += shift;
        }
      }
      if(!session->Configure(candidate)) { fImpl->submitting = false; return false; }
      fImpl->dirty = false;
      fImpl->boundsEditor = 0;
    }
    fImpl->submitting = false;
    Refresh(true);
    if(calculate) return session->Refresh();
    return true;
  } catch(const std::exception& error) {
    fImpl->submitting = false;
    session->SetStatus(error.what());
    return false;
  }
}

// ============== ProcessMessage ==============
// Purpose: Route ROOT widget messages to one session without intercepting native canvas events.
// Inputs: Native message/IDs. Outputs: Updated drafts, settings, or view.
Bool_t Window::ProcessMessage(Long_t message, Long_t first, Long_t second) {
  auto* session = fImpl->session;
  if(!session || fImpl->refreshing) return kTRUE;
  const int type = GET_MSG(message);
  const int action = GET_SUBMSG(message);
  if(type == kC_TEXTENTRY) {
    if(action == kTE_TEXTCHANGED) {
      if(first == kLow || first == kHigh || first == kCenter || first == kWidth)
        fImpl->boundsEditor = static_cast<int>(first);
      fImpl->dirty = true; session->DraftChanged();
    }
    if(action == kTE_ENTER) {
      if(!fImpl->dirty) return kTRUE;
      if(Submit(false) && session->Auto()) session->Refresh();
    }
    return kTRUE;
  }
  if(first >= 1000) {
    auto found = std::find_if(fImpl->rows.begin(), fImpl->rows.end(), [&](const auto& row) {
      return first == row.selectId || first == row.enabledId || first == row.sliderId || first == row.deleteId;
    });
    if(found == fImpl->rows.end()) return kTRUE;
    const auto row = *found;
    if(type == kC_COMMAND && first == row.deleteId) {
      session->Select(row.background, row.id); session->DeleteSelected(); return kTRUE;
    }
    if(type == kC_HSLIDER) {
      if(action == kSL_PRESS) {
        fImpl->sliderBlocked = !Submit(false);
        if(!fImpl->sliderBlocked) { session->Select(row.background, row.id); session->SetGesture(true); }
      }
      if(action == kSL_POS && !fImpl->sliderBlocked) {
        const bool relative = row.background && session->Configuration().background.selection == BackgroundTemplate::ManualRelative;
        const double shift = relative ? session->Anchor() : 0.0;
        session->SetBounds(row.slider->GetMinPositionD() - shift, row.slider->GetMaxPositionD() - shift, true);
      }
      if(action == kSL_RELEASE && !fImpl->sliderBlocked) { session->SetGesture(false); session->Render(); }
      return kTRUE;
    }
    const bool enabled = row.enabled->IsOn();
    if(type == kC_COMMAND && Submit(false)) {
      session->Select(row.background, row.id);
      if(!row.background && first == row.selectId && session->Configuration().combination == Combination::Separate) {
        std::size_t index = 0;
        for(const auto& gate : session->Configuration().gates) if(gate.enabled) {
          if(gate.id == row.id) { session->SetResultIndex(index); break; }
          ++index;
        }
      }
      if(first == row.enabledId) {
        auto config = session->Configuration();
        auto& list = row.background ? config.background.gates : config.gates;
        for(auto& gate : list) if(gate.id == row.id) gate.enabled = enabled;
        if(session->Configure(config) && session->Auto()) session->Refresh();
      }
    }
    return kTRUE;
  }
  if(type != kC_COMMAND) return kTRUE;
  if(first == kExit) { CloseWindow(); return kTRUE; }
  if(first == kUpdate) { Submit(true); return kTRUE; }
  if(first == kAuto) { session->SetAuto(fImpl->checks.at(kAuto)->IsOn()); return kTRUE; }
  if(first == kBrowse || first == kPick) { session->SetBrowse(first == kBrowse); return kTRUE; }
  if(first == kRaw || first == kSubtracted || first == kNet) {
    if(!fImpl->checks.at(kRaw)->IsOn() && !fImpl->checks.at(kSubtracted)->IsOn() && !fImpl->checks.at(kNet)->IsOn())
      fImpl->checks.at(kNet)->SetOn(true, false);
    session->SetView(fImpl->checks.at(kRaw)->IsOn(), fImpl->checks.at(kSubtracted)->IsOn(), fImpl->checks.at(kNet)->IsOn());
    return kTRUE;
  }
  if(first == kResult && action == kCM_COMBOBOX) { session->SetResultIndex(std::max(0L, second - 1)); return kTRUE; }
  if(first == kDelete) { session->DeleteSelected(); return kTRUE; }
  if(first == kAddSignal || first == kAddBackground || first == kAutomaticCurve) {
    if(!Submit(false)) return kTRUE;
    if(first == kAutomaticCurve) {
      auto config = session->Configuration(); config.background.projectionBackground.clear();
      session->Configure(config); Refresh(true);
    } else session->AddGate(first == kAddBackground);
    return kTRUE;
  }
  if(first == kAxis && action == kCM_COMBOBOX) {
    const auto axis = static_cast<Axis>(second);
    if(Submit(false)) session->SwitchAxis(axis);
    else fImpl->combos.at(kAxis)->Select(session->Configuration().gateAxis == Axis::X ? 0 : 1, false);
    return kTRUE;
  }
  fImpl->dirty = true;
  session->DraftChanged();
  if(Submit(false) && session->Auto()) session->Refresh();
  return kTRUE;
}

// ============== Refresh ==============
// Purpose: Paint authoritative state without replacing incomplete text.
// Inputs: Optional draft discard. Outputs: Synchronized controls, actual bins, and report.
void Window::Refresh(bool discardEdits) {
  auto* session = fImpl->session;
  if(!session || fImpl->refreshing || fImpl->submitting) return;
  if(discardEdits) { fImpl->dirty = false; fImpl->boundsEditor = 0; }
  fImpl->refreshing = true;
  fImpl->status->SetText(session->Status().c_str());
  fImpl->checks.at(kAuto)->SetOn(session->Auto(), false);
  fImpl->buttons.at(kPick)->SetState(session->Browse() ? kButtonUp : kButtonDown, false);
  fImpl->buttons.at(kBrowse)->SetState(session->Browse() ? kButtonDown : kButtonUp, false);
  const auto reportText = std::string("Session: ") + (session->Current() ? "CURRENT" : "OUT OF DATE") +
    "\n" + session->Status() + (session->Current() ? "\n\nCurrent calculation:\n" : "\n\nLast successful calculation (historical):\n") +
    session->CurrentResult().Report();
  if(reportText != fImpl->reportText) {
    fImpl->reportText = reportText;
    fImpl->report->LoadBuffer(reportText.c_str());
  }
  if(!fImpl->dirty) {
    const auto& config = session->Configuration();
    const auto& bg = config.background;
    auto select = [&](int id, int value) { fImpl->combos.at(id)->Select(value, false); };
    auto number = [&](int id, double value) { SetNumber(fImpl->entries.at(id), value); };
    auto check = [&](int id, bool value) { fImpl->checks.at(id)->SetOn(value, false); };
    select(kAxis, config.gateAxis == Axis::X ? 0 : 1);
    select(kCombination, static_cast<int>(config.combination));
    select(kModel, static_cast<int>(bg.model)); select(kTemplate, static_cast<int>(bg.selection));
    select(kFractionMethod, static_cast<int>(bg.fractionMethod)); select(kErrors, static_cast<int>(config.errors));
    select(kIntegral, static_cast<int>(bg.fractionIntegral)); select(kOverlap, static_cast<int>(bg.manualOverlap));
    number(kFraction, bg.fraction); number(kUncertainty, bg.relativeUncertainty);
    number(kIterations, bg.spectrumIterations); number(kComptonGap, bg.comptonGapBins);
    fImpl->entries.at(kOptions)->SetText(bg.spectrumOptions.c_str(), false);
    check(kSymmetric, bg.symmetricGammaGamma); check(kUseFitRange, bg.useFitRange); check(kUseNormRange, bg.useNormalizationRange);
    number(kFitLow, bg.fitLow); number(kFitHigh, bg.fitHigh); number(kNormLow, bg.normalizationLow); number(kNormHigh, bg.normalizationHigh);
    check(kGateUnder, config.flow.gateUnderflow); check(kGateOver, config.flow.gateOverflow);
    check(kResultUnder, config.flow.resultUnderflow); check(kResultOver, config.flow.resultOverflow);
    const bool radware = bg.model == SubtractionModel::RadWareStandard;
    const bool active = radware || bg.selection != BackgroundTemplate::None;
    const bool manual = !radware && (bg.selection == BackgroundTemplate::Manual || bg.selection == BackgroundTemplate::ManualRelative);
    const bool fit = active && !radware && (bg.fractionMethod == FractionMethod::LinearFixed || bg.fractionMethod == FractionMethod::PolynomialFlex);
    const bool spectrum = active && (radware || bg.fractionMethod == FractionMethod::TSpectrum);
    const bool normalization = active && !radware && bg.fractionMethod != FractionMethod::Width;
    fImpl->combos.at(kTemplate)->SetEnabled(!radware);
    fImpl->combos.at(kFractionMethod)->SetEnabled(active && !radware);
    fImpl->combos.at(kIntegral)->SetEnabled(active && !radware && bg.fractionMethod != FractionMethod::Width && bg.fractionMethod != FractionMethod::Manual);
    fImpl->combos.at(kOverlap)->SetEnabled(manual);
    fImpl->entries.at(kFraction)->SetEnabled(active && !radware && bg.fractionMethod == FractionMethod::Manual);
    fImpl->entries.at(kUncertainty)->SetEnabled(active);
    fImpl->entries.at(kIterations)->SetEnabled(spectrum && bg.projectionBackground.empty());
    fImpl->entries.at(kOptions)->SetEnabled(spectrum && bg.projectionBackground.empty());
    fImpl->entries.at(kComptonGap)->SetEnabled(active && !radware && bg.selection == BackgroundTemplate::Compton);
    auto enableCheck = [&](int id, bool enabled, bool value) {
      auto* button = fImpl->checks.at(id);
      button->SetEnabled(true); button->SetOn(value, false);
      if(!enabled) button->SetEnabled(false);
    };
    enableCheck(kSymmetric, radware, bg.symmetricGammaGamma);
    enableCheck(kUseFitRange, fit, bg.useFitRange);
    enableCheck(kUseNormRange, normalization, bg.useNormalizationRange);
    for(int id : {kFitLow, kFitHigh}) fImpl->entries.at(id)->SetEnabled(fit && bg.useFitRange);
    for(int id : {kNormLow, kNormHigh}) fImpl->entries.at(id)->SetEnabled(normalization && bg.useNormalizationRange);
    fImpl->buttons.at(kAutomaticCurve)->SetEnabled(spectrum && !bg.projectionBackground.empty());
    fImpl->curve->SetText(bg.projectionBackground.empty() ? "Automatic curve" :
      TString::Format("Imported curve: %zu bins", bg.projectionBackground.size()).Data());
    // Keep surviving native widgets alive; callbacks bind stable IDs rather than row numbers.
    auto matches = [&](const Impl::GateRow& row) {
      if(row.axis != config.gateAxis) return false;
      const auto& list = row.background ? bg.gates : config.gates;
      return std::any_of(list.begin(), list.end(), [&](const Gate& gate) { return gate.id == row.id; });
    };
    auto& rows = fImpl->rows;
    rows.erase(std::remove_if(rows.begin(), rows.end(), [&](auto& row) {
      if(matches(row)) return false;
      row.frame->UnmapWindow();
      fImpl->gateRows->RemoveFrame(row.frame);
      fImpl->retiredRows.push_back({std::unique_ptr<TGVerticalFrame>(row.frame),
        static_cast<unsigned long long>(gSystem->Now()) + 150});
      return true;
    }), rows.end());
    for(int kind = 0; kind < 2; ++kind) {
      const auto& list = kind ? bg.gates : config.gates;
      std::size_t ordinal = 0;
      for(const auto& gate : list) {
        ++ordinal;
        auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.id == gate.id && row.background == (kind != 0); });
        if(found == rows.end()) {
          Impl::GateRow row;
          row.id = gate.id; row.background = kind != 0; row.axis = config.gateAxis;
          row.selectId = fImpl->nextWidgetId++; row.enabledId = fImpl->nextWidgetId++;
          row.sliderId = fImpl->nextWidgetId++; row.deleteId = fImpl->nextWidgetId++;
          row.frame = new TGVerticalFrame(fImpl->gateRows, 550, 58);
          row.frame->SetCleanup(kDeepCleanup);
          row.frame->SetName(TString::Format("MatrixGate%s_%llu", kind ? "BG" : "Signal", static_cast<unsigned long long>(gate.id)));
          auto* heading = new TGHorizontalFrame(row.frame);
          row.frame->AddFrame(heading, new TGLayoutHints(kLHintsExpandX));
          row.select = new TGTextButton(heading, "", row.selectId); row.select->Associate(this);
          row.select->SetToolTipText("Select this gate for numeric editing. Drag either slider handle to resize, or the middle to translate.");
          heading->AddFrame(row.select, new TGLayoutHints(kLHintsExpandX, 2, 2, 1, 1));
          row.enabled = new TGCheckButton(heading, "On", row.enabledId); row.enabled->Associate(this);
          heading->AddFrame(row.enabled, new TGLayoutHints(kLHintsCenterY, 2, 2));
          auto* remove = new TGTextButton(heading, "Delete", row.deleteId); remove->Associate(this);
          heading->AddFrame(remove, new TGLayoutHints(kLHintsCenterY, 2, 2));
          row.slider = new TGDoubleHSlider(row.frame, 530, kDoubleScaleBoth, row.sliderId);
          row.slider->Associate(this);
          row.frame->AddFrame(row.slider, new TGLayoutHints(kLHintsExpandX, 6, 6, 1, 2));
          fImpl->gateRows->AddFrame(row.frame, new TGLayoutHints(kLHintsExpandX, 1, 1, 1, 2));
          rows.push_back(row); found = std::prev(rows.end());
          row.frame->MapSubwindows(); row.frame->MapWindow();
        }
        auto& row = *found;
        const bool selected = gate.id == session->SelectedId() && (kind != 0) == session->SelectedBackground();
        row.select->SetText(TString::Format("%s%zu  %s  [%.6g, %.6g)", kind ? "BG" : "Signal", ordinal, gate.name.c_str(), gate.low, gate.high));
        row.select->SetTextColor(TColor::Number2Pixel(session->GateColor(gate.id)));
        row.select->SetState(selected ? kButtonDown : kButtonUp, false);
        row.enabled->SetOn(gate.enabled, false);
        auto limits = session->GateLimits(kind != 0);
        const double shift = kind && bg.selection == BackgroundTemplate::ManualRelative ? session->Anchor() : 0.0;
        row.slider->SetRange(limits.first + shift, limits.second + shift);
        row.slider->SetPosition(gate.low + shift, gate.high + shift);
        if(auto* color = gROOT->GetColor(session->GateColor(gate.id))) {
          const int tint = TColor::GetColor(0.88f + 0.12f * color->GetRed(),
            0.88f + 0.12f * color->GetGreen(), 0.88f + 0.12f * color->GetBlue());
          row.slider->SetBackgroundColor(TColor::Number2Pixel(tint));
        }
      }
    }
    fImpl->gateRows->Resize(std::max(100u, fImpl->gateViewport->GetViewPort()->GetWidth()),
      std::max(1u, fImpl->gateRows->GetDefaultHeight()));
    fImpl->gateRows->Layout(); fImpl->gateViewport->Layout();
    const auto* gate = session->SelectedGate();
    fImpl->editedId = gate ? gate->id : 0; fImpl->editedBackground = session->SelectedBackground();
    const auto edges = session->GateEdges();
    const bool relative = session->SelectedBackground() && bg.selection == BackgroundTemplate::ManualRelative;
    for(const int id : {kName, kLow, kHigh, kCenter, kWidth}) fImpl->entries.at(id)->SetEnabled(gate != nullptr);
    fImpl->checks.at(kEnabled)->SetEnabled(gate != nullptr);
    if(gate) {
      fImpl->entries.at(kName)->SetText(gate->name.c_str(), false);
      number(kLow, gate->low); number(kHigh, gate->high);
      number(kCenter, (gate->low + gate->high) / 2.0); number(kWidth, gate->high - gate->low);
      check(kEnabled, gate->enabled);
      try {
        auto actualGate = *gate;
        if(relative) { actualGate.low += session->Anchor(); actualGate.high += session->Anchor(); }
        const auto actual = ResolveGate(actualGate, edges, config.flow);
        fImpl->actual->SetText(TString::Format("%sActual bins %d..%d: [%.9g, %.9g), width %.9g",
          relative ? "Offsets from signal center. " : "", actual.first, actual.last, actual.low, actual.high, actual.width));
      } catch(const std::exception& error) { fImpl->actual->SetText(error.what()); }
    } else {
      for(const int id : {kLow, kHigh, kCenter, kWidth}) number(id, 0);
      fImpl->entries.at(kName)->SetText("", false); check(kEnabled, false);
      fImpl->actual->SetText("No gate selected. Pick two edges on the total projection or add a gate.");
    }
    fImpl->combos.at(kResult)->RemoveAll();
    const auto& results = session->CurrentResult().projections;
    for(std::size_t index = 0; index < results.size(); ++index) {
      const auto& projection = results[index];
      std::ostringstream name;
      name << index + 1 << ": " << ToString(session->CurrentResult().config.combination);
      for(const auto& selected : projection.gates) name << " | " << selected.requested.id << ' ' << selected.requested.name;
      fImpl->combos.at(kResult)->AddEntry(name.str().c_str(), static_cast<int>(index + 1));
    }
    if(!results.empty()) select(kResult, static_cast<int>(std::min(session->ResultIndex(), results.size() - 1) + 1));
  }
  Layout();
  fImpl->refreshing = false;
}

} // namespace MatrixGate::Detail

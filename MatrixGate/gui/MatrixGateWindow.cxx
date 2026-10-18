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
#include <TGListBox.h>
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

} // namespace

class Window::Impl {
public:
  Session* session = nullptr;
  TRootEmbeddedCanvas* total = nullptr;
  TRootEmbeddedCanvas* result = nullptr;
  TGListBox* gates = nullptr;
  TGLabel* target = nullptr;
  TGLabel* status = nullptr;
  TGLabel* actual = nullptr;
  TGLabel* curve = nullptr;
  MatrixGateReportView* report = nullptr;
  TGDoubleHSlider* slider = nullptr;
  std::map<int, TGTextEntry*> entries;
  std::map<int, TGComboBox*> combos;
  std::map<int, TGCheckButton*> checks;
  std::map<int, TGTextButton*> buttons;
  std::vector<std::pair<bool, std::uint64_t>> rows;
  std::array<std::array<BackgroundConfig, 3>, 2> modelCache;
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
  check(toolbar, "Raw", kRaw); check(toolbar, "Background", kSubtracted); check(toolbar, "Net", kNet);
  fImpl->checks.at(kNet)->SetOn(true, false);
  button(toolbar, "Close", kExit);
  auto* canvases = new TGHorizontalFrame(this, 1300, 410, kFixedHeight);
  fImpl->result = new TRootEmbeddedCanvas("MatrixGateResult", canvases, 640, 410);
  fImpl->total = new TRootEmbeddedCanvas("MatrixGateTotal", canvases, 640, 410);
  canvases->AddFrame(fImpl->result, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 2, 2, 2, 2));
  canvases->AddFrame(fImpl->total, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 2, 2, 2, 2));
  AddFrame(canvases, new TGLayoutHints(kLHintsExpandX, 4, 4, 0, 0));
  auto* resultRow = row(this);
  combo(resultRow, "Shown result", kResult, {}, 460);
  label(resultRow, "Signal: blue dashed | Background gates: green dashed | Double-click: move center");
  auto* tabs = new TGTab(this, 1280, 260);
  AddFrame(tabs, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 6, 6, 2, 2));
  auto* gates = tabs->AddTab("Gates");
  auto* actions = row(gates);
  button(actions, "+ Signal", kAddSignal); button(actions, "+ Background", kAddBackground);
  button(actions, "Delete selected", kDelete);
  label(actions, "Pick two edges edits the selected gate; with no selection it creates a signal gate.");
  auto* editors = row(gates);
  fImpl->gates = new TGListBox(editors, kGateList);
  fImpl->gates->Associate(this); fImpl->gates->Resize(550, 165);
  editors->AddFrame(fImpl->gates, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 3, 8, 2, 2));
  auto* settings = new TGVerticalFrame(editors, 680, 180);
  editors->AddFrame(settings, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
  auto* identity = row(settings);
  entry(identity, "Name", kName, 185); check(identity, "Enabled", kEnabled);
  auto* bounds = row(settings);
  entry(bounds, "Low", kLow); entry(bounds, "High", kHigh);
  auto* center = row(settings);
  entry(center, "Center", kCenter); entry(center, "Width", kWidth);
  fImpl->slider = new TGDoubleHSlider(settings, 600, kDoubleScaleBoth, kSlider);
  fImpl->slider->Associate(this);
  settings->AddFrame(fImpl->slider, new TGLayoutHints(kLHintsExpandX, 12, 12, 4, 3));
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
  fImpl->status = new TGLabel(this, "Ready");
  fImpl->status->SetTextJustify(kTextLeft); fImpl->status->SetWrapLength(1250);
  AddFrame(fImpl->status, new TGLayoutHints(kLHintsExpandX, 10, 10, 5, 7));
  Refresh(true);
}

// ============== ~Window ==============
// Purpose: Release native child widgets through ROOT cleanup. Inputs: Owned frame. Outputs: Closed window.
Window::~Window() = default;

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
      bg.fraction = Number(e(kFraction), "Manual fraction");
      bg.relativeUncertainty = Number(e(kUncertainty), "Relative uncertainty");
      bg.spectrumIterations = Integer(e(kIterations), "Iterations");
      bg.comptonGapBins = Integer(e(kComptonGap), "Compton gap");
      bg.spectrumOptions = e(kOptions)->GetText();
      bg.fractionIntegral = static_cast<FractionIntegral>(c(kIntegral));
      bg.manualOverlap = static_cast<ManualOverlap>(c(kOverlap));
      bg.symmetricGammaGamma = b(kSymmetric);
      bg.useFitRange = b(kUseFitRange); bg.fitLow = Number(e(kFitLow), "Fit low"); bg.fitHigh = Number(e(kFitHigh), "Fit high");
      bg.useNormalizationRange = b(kUseNormRange);
      bg.normalizationLow = Number(e(kNormLow), "Normalization low"); bg.normalizationHigh = Number(e(kNormHigh), "Normalization high");
      candidate.errors = static_cast<ErrorPolicy>(c(kErrors));
      candidate.flow = {b(kGateUnder), b(kGateOver), b(kResultUnder), b(kResultOver)};
      const auto& previous = session->Configuration().background;
      const int axis = candidate.gateAxis == Axis::X ? 0 : 1;
      if(bg.model != previous.model) {
        auto saved = bg; saved.model = previous.model;
        fImpl->modelCache[axis][static_cast<int>(previous.model)] = saved;
        fImpl->errorCache[axis][static_cast<int>(previous.model)] = candidate.errors;
        const int model = static_cast<int>(bg.model);
        bg = fImpl->modelCache[axis][model];
        candidate.errors = fImpl->errorCache[axis][model];
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
  if(type == kC_HSLIDER && first == kSlider) {
    if(action == kSL_PRESS) { fImpl->sliderBlocked = !Submit(false); if(!fImpl->sliderBlocked) session->SetGesture(true); }
    if(action == kSL_POS && !fImpl->sliderBlocked) {
      session->SetBounds(fImpl->slider->GetMinPositionD(), fImpl->slider->GetMaxPositionD(), true);
    }
    if(action == kSL_RELEASE && !fImpl->sliderBlocked) { session->SetGesture(false); session->Render(); }
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
  if(first == kGateList && action == kCM_LISTBOX) {
    if(!Submit(false)) return kTRUE;
    if(second > 0 && static_cast<std::size_t>(second) <= fImpl->rows.size()) {
      const auto selected = fImpl->rows[second - 1];
      session->Select(selected.first, selected.second);
      if(!selected.first && session->Configuration().combination == Combination::Separate) {
        const auto& results = session->CurrentResult().projections;
        for(std::size_t index = 0; index < results.size(); ++index)
          if(!results[index].gates.empty() && results[index].gates.front().requested.id == selected.second)
            session->SetResultIndex(index);
      }
    }
    return kTRUE;
  }
  if(first == kAddSignal || first == kAddBackground || first == kDelete || first == kAutomaticCurve) {
    if(!Submit(false)) return kTRUE;
    if(first == kDelete) session->DeleteSelected();
    else if(first == kAutomaticCurve) {
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
  const auto reportText = session->CurrentResult().Report() + "\nSession: " +
    (session->Current() ? "CURRENT" : "OUT OF DATE") + "\n" + session->Status();
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
    fImpl->curve->SetText(bg.projectionBackground.empty() ? "Automatic curve" :
      TString::Format("Imported curve: %zu bins", bg.projectionBackground.size()).Data());
    fImpl->rows.clear(); fImpl->gates->RemoveAll();
    int chosenRow = -1;
    for(int kind = 0; kind < 2; ++kind) {
      const auto& gates = kind ? bg.gates : config.gates;
      for(const auto& gate : gates) {
        fImpl->rows.push_back({kind != 0, gate.id});
        const int rowId = static_cast<int>(fImpl->rows.size());
        fImpl->gates->AddEntry(TString::Format("%s %llu  %s  [%.7g, %.7g) %s",
          kind ? "BG" : "Signal", static_cast<unsigned long long>(gate.id), gate.name.c_str(), gate.low, gate.high,
          gate.enabled ? "" : "(disabled)"), rowId);
        if(session->SelectedId() == gate.id && session->SelectedBackground() == (kind != 0)) chosenRow = rowId;
      }
    }
    if(chosenRow > 0) { fImpl->gates->Select(chosenRow); fImpl->gates->SetTopEntry(chosenRow); }
    const auto* gate = session->SelectedGate();
    fImpl->editedId = gate ? gate->id : 0; fImpl->editedBackground = session->SelectedBackground();
    const auto edges = session->GateEdges();
    const bool relative = session->SelectedBackground() && bg.selection == BackgroundTemplate::ManualRelative;
    const double span = edges.back() - edges.front();
    fImpl->slider->SetRange(relative ? -span : edges.front(), relative ? span : edges.back());
    for(const int id : {kName, kLow, kHigh, kCenter, kWidth}) fImpl->entries.at(id)->SetEnabled(gate != nullptr);
    fImpl->checks.at(kEnabled)->SetEnabled(gate != nullptr);
    if(gate) {
      fImpl->entries.at(kName)->SetText(gate->name.c_str(), false);
      number(kLow, gate->low); number(kHigh, gate->high);
      number(kCenter, (gate->low + gate->high) / 2.0); number(kWidth, gate->high - gate->low);
      check(kEnabled, gate->enabled); fImpl->slider->SetPosition(gate->low, gate->high);
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

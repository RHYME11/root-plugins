#include <PhotoPeakFit/PhotoPeakControlWindow.h>

#include <algorithm>
#include <array>
#include <sstream>
#include <set>
#include <vector>

#include <TGButton.h>
#include <TGCanvas.h>
#include <TGComboBox.h>
#include <TGLabel.h>
#include <TGMenu.h>
#include <TGListBox.h>
#include <TGTab.h>
#include <TGTextView.h>
#include <TH1.h>
#include <TCanvas.h>
#include <TVirtualPad.h>
#include <cmath>
#include <iomanip>
#include <TGTextEntry.h>
#include <TString.h>
#include <TVirtualX.h>

#include <PhotoPeakFit/PhotoPeakSession.h>

#include "PhotoPeakParameterRow.h"
#include "PhotoPeakEntry.h"

namespace {
enum ControlId {
  kFit = 100,
  kClean,
  kExitMode,
  kAddPeak,
  kApply,
  kMode,
  kBackground,
  kBackgroundOptions,
  kRelativePosition,
  kRelativeFwhm,
  kPeakList = 110,
  kPickPeaks,
  kSetRange,
  kBrowse,
  kDeletePeak,
  kRebin = 120,
  kUndoRebin,
  kPanLeft,
  kPanRight,
  kUnzoom,
  kPreview,
  kBackDecreasingWindow = 500,
  kBackIncreasingWindow,
  kBackOrder2,
  kBackOrder4,
  kBackOrder6,
  kBackOrder8,
  kBackNoSmoothing,
  kBackSmoothing3,
  kBackSmoothing5,
  kBackSmoothing7,
  kBackSmoothing9,
  kBackSmoothing11,
  kBackSmoothing13,
  kBackSmoothing15,
  kBackCompton
};

struct BackgroundOption {
  int id;
  const char* token;
};

const BackgroundOption kBackgroundOptionList[] = {
  {kBackDecreasingWindow, "BackDecreasingWindow"},
  {kBackIncreasingWindow, "BackIncreasingWindow"},
  {kBackOrder2, "BackOrder2"},
  {kBackOrder4, "BackOrder4"},
  {kBackOrder6, "BackOrder6"},
  {kBackOrder8, "BackOrder8"},
  {kBackNoSmoothing, "nosmoothing"},
  {kBackSmoothing3, "BackSmoothing3"},
  {kBackSmoothing5, "BackSmoothing5"},
  {kBackSmoothing7, "BackSmoothing7"},
  {kBackSmoothing9, "BackSmoothing9"},
  {kBackSmoothing11, "BackSmoothing11"},
  {kBackSmoothing13, "BackSmoothing13"},
  {kBackSmoothing15, "BackSmoothing15"},
  {kBackCompton, "Compton"}
};

// ============== BackgroundOptionsFromMenu ==============
// Purpose: Serialize checked TSpectrum background options.
// Inputs: Background option popup menu.
// Outputs: Space-separated ROOT option string.
std::string BackgroundOptionsFromMenu(TGPopupMenu* menu) {
  std::string options;
  for(const auto& option : kBackgroundOptionList) {
    if(menu->IsEntryChecked(option.id)) {
      if(!options.empty())
        options += ' ';
      options += option.token;
    }
  }
  return options;
}

// ============== SetBackgroundOptions ==============
// Purpose: Synchronize the popup checks from a ROOT option string.
// Inputs: Popup menu and space-separated options.
// Outputs: Updated check marks.
void SetBackgroundOptions(TGPopupMenu* menu, const std::string& options) {
  std::istringstream input(options);
  std::set<std::string> selected;
  for(std::string token; input >> token;)
    selected.insert(token);
  for(const auto& option : kBackgroundOptionList) {
    if(selected.count(option.token))
      menu->CheckEntry(option.id);
    else
      menu->UnCheckEntry(option.id);
  }
}

// ============== ToggleBackgroundOption ==============
// Purpose: Toggle one option and enforce mutually exclusive ROOT groups.
// Inputs: Popup menu and selected option identifier.
// Outputs: Consistent check marks.
void ToggleBackgroundOption(TGPopupMenu* menu, int identifier) {
  const int direction[] = {kBackDecreasingWindow, kBackIncreasingWindow};
  const int order[] = {kBackOrder2, kBackOrder4, kBackOrder6, kBackOrder8};
  const int smoothing[] = {kBackNoSmoothing, kBackSmoothing3, kBackSmoothing5,
    kBackSmoothing7, kBackSmoothing9, kBackSmoothing11, kBackSmoothing13,
    kBackSmoothing15};
  auto toggleGroup = [&](const int* values, std::size_t count) {
    const auto found = std::find(values, values + count, identifier);
    if(found == values + count)
      return false;
    for(std::size_t index = 0; index < count; ++index)
      menu->UnCheckEntry(values[index]);
    menu->CheckEntry(identifier);
    return true;
  };
  if(toggleGroup(direction, std::size(direction)) ||
     toggleGroup(order, std::size(order)) ||
     toggleGroup(smoothing, std::size(smoothing)))
    return;
  if(menu->IsEntryChecked(identifier))
    menu->UnCheckEntry(identifier);
  else
    menu->CheckEntry(identifier);
}
}

class PhotoPeakControlWindow::Impl {
public:
  PhotoPeakSession* session = nullptr;
  TGLabel* target = nullptr;
  TGLabel* status = nullptr;
  TGLabel* selected = nullptr;
  TGLabel* activeParameters = nullptr;
  TGLabel* resultStatus = nullptr;
  TGListBox* peakList = nullptr;
  TGTab* tabs = nullptr;
  PhotoPeakEntry* rangeLow = nullptr;
  PhotoPeakEntry* rangeHigh = nullptr;
  PhotoPeakEntry* backgroundIterations = nullptr;
  PhotoPeakEntry* backgroundLow = nullptr;
  PhotoPeakEntry* backgroundHigh = nullptr;
  TGTextEntry* rootOptions = nullptr;
  TGComboBox* mode = nullptr;
  TGComboBox* background = nullptr;
  TGCheckButton* relativePosition = nullptr;
  TGCheckButton* relativeFwhm = nullptr;
  TGTextButton* backgroundOptions = nullptr;
  std::unique_ptr<TGPopupMenu> backgroundOptionMenu;
  std::array<std::unique_ptr<PhotoPeakParameterRow>, 3> peakRows;
  std::array<std::unique_ptr<PhotoPeakParameterRow>, 7> globals;
  std::array<TGTextButton*, 3> tools{};
  TGTextView* results = nullptr;
  std::uint64_t editedPeak = 0;
  bool dirty = false;
  bool refreshing = false;
  bool committing = false;
  int activeTab = 0;
};

// ============== PhotoPeakControlWindow::PhotoPeakControlWindow ==============
// Purpose: Build an English ROOT-native control panel linked to the existing canvas.
// Inputs: Owning session. Outputs: Four configuration pages and persistent actions.
PhotoPeakControlWindow::PhotoPeakControlWindow(PhotoPeakSession* session)
  : TGMainFrame(gClient->GetRoot(), 480, 700), fImpl(std::make_unique<Impl>()) {
  fImpl->session = session;
  SetWindowName("PhotoPeakFit - Controls");
  SetCleanup(kDeepCleanup);
  auto addButton = [&](TGCompositeFrame* parent, const char* text, int id) {
    auto* button = new TGTextButton(parent, text, id);
    button->Associate(this);
    parent->AddFrame(button, new TGLayoutHints(kLHintsLeft, 3, 3, 3, 3));
    return button;
  };
  auto addEntry = [&](TGCompositeFrame* parent, const char* label, int id) {
    auto* row = new TGHorizontalFrame(parent);
    row->AddFrame(new TGLabel(row, label), new TGLayoutHints(kLHintsCenterY, 4, 6, 3, 3));
    auto* entry = new PhotoPeakEntry(row, id, this);
    row->AddFrame(entry, new TGLayoutHints(kLHintsExpandX, 3, 3, 3, 3));
    parent->AddFrame(row, new TGLayoutHints(kLHintsExpandX));
    return entry;
  };
  fImpl->target = new TGLabel(this, "Target");
  fImpl->target->SetTextJustify(kTextLeft);
  fImpl->target->SetWrapLength(450);
  AddFrame(fImpl->target, new TGLayoutHints(kLHintsExpandX, 8, 8, 6, 2));
  auto* tools = new TGHorizontalFrame(this);
  fImpl->tools[0] = addButton(tools, "Pick peaks", kPickPeaks);
  fImpl->tools[1] = addButton(tools, "Set range", kSetRange);
  fImpl->tools[2] = addButton(tools, "Browse", kBrowse);
  AddFrame(tools, new TGLayoutHints(kLHintsExpandX, 5, 5));
  auto* range = new TGHorizontalFrame(this);
  range->AddFrame(new TGLabel(range, "Fit low"), new TGLayoutHints(kLHintsCenterY, 4, 4, 4, 4));
  fImpl->rangeLow = new PhotoPeakEntry(range, 10, this);
  range->AddFrame(fImpl->rangeLow, new TGLayoutHints(kLHintsExpandX, 2, 6, 4, 4));
  range->AddFrame(new TGLabel(range, "High"), new TGLayoutHints(kLHintsCenterY, 4, 4, 4, 4));
  fImpl->rangeHigh = new PhotoPeakEntry(range, 11, this);
  range->AddFrame(fImpl->rangeHigh, new TGLayoutHints(kLHintsExpandX, 2, 6, 4, 4));
  addButton(range, "Apply", kApply);
  AddFrame(range, new TGLayoutHints(kLHintsExpandX, 5, 5));

  fImpl->tabs = new TGTab(this, 460, 490);
  fImpl->tabs->Associate(this);
  auto* peaks = fImpl->tabs->AddTab("Peaks");
  auto* actions = new TGHorizontalFrame(peaks);
  addButton(actions, "+ Add peak", kAddPeak);
  addButton(actions, "Delete selected", kDeletePeak);
  peaks->AddFrame(actions, new TGLayoutHints(kLHintsExpandX, 3, 3, 4, 2));
  fImpl->peakList = new TGListBox(peaks, kPeakList);
  fImpl->peakList->Associate(this);
  fImpl->peakList->Resize(440, 110);
  peaks->AddFrame(fImpl->peakList, new TGLayoutHints(kLHintsExpandX, 6, 6, 2, 6));
  fImpl->selected = new TGLabel(peaks, "No peak selected");
  peaks->AddFrame(fImpl->selected, new TGLayoutHints(kLHintsLeft, 8, 4, 2, 4));
  const char* peakNames[] = {"Centroid", "Height", "FWHM"};
  for(int index = 0; index < 3; ++index)
    fImpl->peakRows[index] = std::make_unique<PhotoPeakParameterRow>(peaks, peakNames[index], 1000 + index * 10, this);
  auto* settings = new TGHorizontalFrame(peaks);
  settings->AddFrame(new TGLabel(settings, "Fit mode"), new TGLayoutHints(kLHintsCenterY, 6, 6, 4, 4));
  fImpl->mode = new TGComboBox(settings, kMode);
  fImpl->mode->AddEntry("Auto", 0);
  fImpl->mode->AddEntry("HighStat", 1);
  fImpl->mode->AddEntry("LowStat", 2);
  fImpl->mode->Resize(120, 24);
  fImpl->mode->Associate(this);
  settings->AddFrame(fImpl->mode, new TGLayoutHints(kLHintsLeft, 2, 4, 4, 4));
  peaks->AddFrame(settings, new TGLayoutHints(kLHintsExpandX));
  fImpl->relativePosition = new TGCheckButton(peaks, "Relative position", kRelativePosition);
  fImpl->relativeFwhm = new TGCheckButton(peaks, "Relative FWHM", kRelativeFwhm);
  for(auto* button : {fImpl->relativePosition, fImpl->relativeFwhm}) {
    button->Associate(this);
    peaks->AddFrame(button, new TGLayoutHints(kLHintsLeft, 6, 4, 3, 3));
  }

  auto* shape = fImpl->tabs->AddTab("Shape");
  auto* shapeLabel = new TGLabel(shape, "Global parameters: Value / Mode");
  shape->AddFrame(shapeLabel, new TGLayoutHints(kLHintsLeft, 6, 4, 5, 4));
  const char* globalNames[] = {"R", "BETA", "STEP", "A", "B", "C", "WSCALE"};
  for(int index = 0; index < 7; ++index)
    fImpl->globals[index] = std::make_unique<PhotoPeakParameterRow>(shape, globalNames[index], 300 + index * 10, this);
  fImpl->activeParameters = new TGLabel(shape, "");
  fImpl->activeParameters->SetWrapLength(440);
  fImpl->activeParameters->SetTextJustify(kTextLeft);
  shape->AddFrame(fImpl->activeParameters, new TGLayoutHints(kLHintsExpandX, 6, 6, 4, 4));
  auto* advanced = new TGGroupFrame(shape, "Advanced ROOT options");
  fImpl->rootOptions = new PhotoPeakEntry(advanced, 16, this);
  advanced->AddFrame(fImpl->rootOptions, new TGLayoutHints(kLHintsExpandX, 4, 4, 4, 4));
  shape->AddFrame(advanced, new TGLayoutHints(kLHintsExpandX, 6, 6, 4, 6));

  auto* background = fImpl->tabs->AddTab("Background");
  background->AddFrame(new TGLabel(background, "TSpectrum preview (separate from fit background)"),
    new TGLayoutHints(kLHintsLeft, 6, 4, 6, 4));
  fImpl->background = new TGComboBox(background, kBackground);
  fImpl->background->AddEntry("None", 0);
  fImpl->background->AddEntry("Local range", 1);
  fImpl->background->AddEntry("Global (full spectrum)", 2);
  fImpl->background->Resize(240, 26);
  fImpl->background->Associate(this);
  background->AddFrame(fImpl->background, new TGLayoutHints(kLHintsLeft, 6, 6, 4, 4));
  fImpl->backgroundIterations = addEntry(background, "Iterations", 12);
  fImpl->backgroundLow = addEntry(background, "Local low", 13);
  fImpl->backgroundHigh = addEntry(background, "Local high", 14);
  fImpl->backgroundOptions = addButton(background, "Options", kBackgroundOptions);
  fImpl->backgroundOptionMenu = std::make_unique<TGPopupMenu>(gClient->GetRoot());
  for(const auto& option : kBackgroundOptionList)
    fImpl->backgroundOptionMenu->AddEntry(option.token, option.id);
  fImpl->backgroundOptionMenu->Associate(this);
  addButton(background, "Preview / clear (b)", kPreview);
  auto* note = new TGLabel(background,
    "None clears the preview. Local uses the specified range.\nGlobal estimates the entire spectrum. A/B/C in Shape\ncontrol the background used by the fit model.");
  note->SetTextJustify(kTextLeft);
  background->AddFrame(note, new TGLayoutHints(kLHintsLeft, 6, 6, 8, 4));

  auto* results = fImpl->tabs->AddTab("Results");
  fImpl->resultStatus = new TGLabel(results, "Not fitted");
  fImpl->resultStatus->SetTextJustify(kTextLeft);
  fImpl->resultStatus->SetWrapLength(440);
  results->AddFrame(fImpl->resultStatus, new TGLayoutHints(kLHintsExpandX, 6, 6, 5, 5));
  fImpl->results = new TGTextView(results, 440, 380);
  results->AddFrame(fImpl->results, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 6, 6, 2, 6));
  AddFrame(fImpl->tabs, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 6, 6, 4, 4));

  auto* view = new TGHorizontalFrame(this);
  addButton(view, "Rebin (w)", kRebin);
  addButton(view, "Undo (q)", kUndoRebin);
  addButton(view, "<", kPanLeft)->SetToolTipText("Pan left (left arrow)");
  addButton(view, ">", kPanRight)->SetToolTipText("Pan right (right arrow)");
  addButton(view, "Unzoom (o)", kUnzoom);
  AddFrame(view, new TGLayoutHints(kLHintsExpandX, 5, 5));
  fImpl->status = new TGLabel(this, "");
  fImpl->status->SetWrapLength(450);
  fImpl->status->SetTextJustify(kTextLeft);
  AddFrame(fImpl->status, new TGLayoutHints(kLHintsExpandX, 8, 8, 5, 5));
  auto* buttons = new TGHorizontalFrame(this);
  addButton(buttons, "Fit (f / Enter)", kFit);
  addButton(buttons, "Clean (n)", kClean);
  addButton(buttons, "Exit", kExitMode);
  AddFrame(buttons, new TGLayoutHints(kLHintsCenterX, 4, 4, 3, 6));
  Refresh(true);
}

// ============== PhotoPeakControlWindow::~PhotoPeakControlWindow ==============
// Purpose: Destroy row adapters before ROOT deletes their widgets.
// Inputs: None. Outputs: Released window-owned adapters.
PhotoPeakControlWindow::~PhotoPeakControlWindow() = default;

// ============== PhotoPeakControlWindow::CommitEdits ==============
// Purpose: Share atomic validation between fields, buttons and canvas shortcuts.
// Inputs: Pending text and controls. Outputs: Valid committed request or retained draft.
bool PhotoPeakControlWindow::CommitEdits() {
  if(fImpl->refreshing || fImpl->committing)
    return true;
  if(!fImpl->session || !fImpl->session->ValidateTarget())
    return false;
  if(!fImpl->dirty)
    return true;
  fImpl->committing = true;
  auto request = fImpl->session->State().request;
  std::string error;
  bool valid = fImpl->rangeLow->Read(request.fitLow, "Fit low", error) &&
    fImpl->rangeHigh->Read(request.fitHigh, "Fit high", error);
  for(auto& peak : request.peaks) {
    if(peak.id == fImpl->editedPeak && valid)
      valid = fImpl->peakRows[0]->Get(peak.centroid, error) &&
        fImpl->peakRows[1]->Get(peak.height, error) && fImpl->peakRows[2]->Get(peak.fwhm, error);
  }
  const int parameterMap[] = {kPhotoPeakR, kPhotoPeakBeta, kPhotoPeakStep, kPhotoPeakA, kPhotoPeakB, kPhotoPeakC};
  for(int index = 0; index < 6 && valid; ++index)
    valid = fImpl->globals[index]->Get(request.config.global[parameterMap[index]], error);
  if(valid)
    valid = fImpl->globals[6]->Get(request.config.widthScale, error);
  double iterations = 0;
  if(valid)
    valid = fImpl->backgroundIterations->Read(iterations, "Background iterations", error) &&
      fImpl->backgroundLow->Read(request.config.backgroundLow, "Local low", error) &&
      fImpl->backgroundHigh->Read(request.config.backgroundHigh, "Local high", error);
  if(valid && (iterations < 1 || iterations > 1000000 || std::floor(iterations) != iterations)) {
    valid = false;
    error = "Background iterations: enter an integer from 1 to 1000000.";
  }
  if(valid) {
    request.config.backgroundIterations = static_cast<int>(iterations);
    request.config.mode = static_cast<PhotoPeakFitMode>(fImpl->mode->GetSelected());
    request.config.background = static_cast<PhotoPeakBackgroundMode>(fImpl->background->GetSelected());
    request.config.backgroundOptions = BackgroundOptionsFromMenu(fImpl->backgroundOptionMenu.get());
    request.config.rootOptions = fImpl->rootOptions->GetText();
    request.config.relativePosition = fImpl->relativePosition->IsOn();
    request.config.relativeFwhm = fImpl->relativeFwhm->IsOn();
    valid = fImpl->session->ReplaceRequest(request);
  } else {
    fImpl->session->SetStatus(error);
  }
  fImpl->committing = false;
  if(valid)
    fImpl->dirty = false;
  Refresh();
  return valid;
}

// ============== PhotoPeakControlWindow::ProcessMessage ==============
// Purpose: Route English controls through the same session commands as canvas input.
// Inputs: ROOT widget message and IDs. Outputs: Validated operation or inline error.
Bool_t PhotoPeakControlWindow::ProcessMessage(Long_t message, Long_t parameter1, Long_t parameter2) {
  if(fImpl->refreshing || !fImpl->session || !fImpl->session->ValidateTarget())
    return kTRUE;
  if(GET_MSG(message) == kC_TEXTENTRY) {
    if(GET_SUBMSG(message) == kTE_TEXTCHANGED) {
      fImpl->dirty = true;
      fImpl->session->SetStatus("Unapplied edits. Enter, Apply or a canvas action validates the input.");
    } else if(GET_SUBMSG(message) == kTE_ENTER || GET_SUBMSG(message) == kTE_TAB) {
      CommitEdits();
    }
    return kTRUE;
  }
  if(GET_MSG(message) != kC_COMMAND)
    return TGMainFrame::ProcessMessage(message, parameter1, parameter2);
  if(fImpl->session->State().fitting)
    return kTRUE;
  if(GET_SUBMSG(message) == kCM_TAB) {
    if(CommitEdits())
      fImpl->activeTab = static_cast<int>(parameter1);
    else
      fImpl->tabs->SetTab(fImpl->activeTab, false);
    return kTRUE;
  }
  if(parameter1 == kClean) { fImpl->session->Clean(); return kTRUE; }
  if(parameter1 == kExitMode) { CloseWindow(); return kTRUE; }
  if(parameter1 == kBrowse) { fImpl->session->SetInteraction(PhotoPeakInteraction::Browse); return kTRUE; }
  if(parameter1 == kBackgroundOptions) {
    Int_t x = 0, y = 0;
    Window_t child = 0;
    gVirtualX->TranslateCoordinates(fImpl->backgroundOptions->GetId(),
      gClient->GetRoot()->GetId(), 0, fImpl->backgroundOptions->GetHeight(), x, y, child);
    fImpl->backgroundOptionMenu->PlaceMenu(x, y, false, true);
    return kTRUE;
  }
  if(GET_SUBMSG(message) == kCM_COMBOBOX || GET_SUBMSG(message) == kCM_CHECKBUTTON ||
     (parameter1 >= kBackDecreasingWindow && parameter1 <= kBackCompton)) {
    if(parameter1 >= kBackDecreasingWindow && parameter1 <= kBackCompton)
      ToggleBackgroundOption(fImpl->backgroundOptionMenu.get(), static_cast<int>(parameter1));
    for(auto& row : fImpl->peakRows)
      row->Update();
    for(auto& row : fImpl->globals)
      row->Update();
    fImpl->dirty = true;
  }
  if(!CommitEdits()) {
    if(parameter1 == kPeakList && fImpl->editedPeak)
      fImpl->peakList->Select(static_cast<int>(fImpl->editedPeak));
    return kTRUE;
  }
  switch(parameter1) {
    case kFit: fImpl->session->Fit(); break;
    case kApply: break;
    case kAddPeak: {
      const auto& request = fImpl->session->State().request;
      fImpl->session->AddPeak(0.5 * (request.fitLow + request.fitHigh));
      break;
    }
    case kDeletePeak: fImpl->session->RemoveSelectedPeak(); break;
    case kPeakList: fImpl->session->SelectPeak(static_cast<std::uint64_t>(parameter2)); break;
    case kPickPeaks: fImpl->session->SetInteraction(PhotoPeakInteraction::Peaks); break;
    case kSetRange: fImpl->session->SetInteraction(PhotoPeakInteraction::Range); break;
    case kRebin: fImpl->session->Rebin(false); break;
    case kUndoRebin: fImpl->session->Rebin(true); break;
    case kPanLeft: fImpl->session->Pan(-1); break;
    case kPanRight: fImpl->session->Pan(1); break;
    case kUnzoom: fImpl->session->Unzoom(); break;
    case kPreview: fImpl->session->ShowBackground(); break;
    default: break;
  }
  return kTRUE;
}

// ============== PhotoPeakControlWindow::CloseWindow ==============
// Purpose: Discard a session through the window close control.
// Inputs: None. Outputs: Closed session and deferred ROOT window destruction.
void PhotoPeakControlWindow::CloseWindow() {
  if(fImpl->session && !fImpl->session->State().fitting)
    fImpl->session->ExitMode();
}

// ============== PhotoPeakControlWindow::Refresh ==============
// Purpose: Render committed state while protecting incomplete input.
// Inputs: Optional discard flag for Clean. Outputs: English controls and result status.
void PhotoPeakControlWindow::Refresh(bool discardEdits) {
  if(!fImpl->session || fImpl->refreshing || fImpl->committing)
    return;
  if(discardEdits)
    fImpl->dirty = false;
  fImpl->refreshing = true;
  const auto& state = fImpl->session->State();
  const auto& request = state.request;
  fImpl->status->SetText(state.status.c_str());
  fImpl->target->SetText(TString::Format("Target: %s | Canvas: %s / %s | Rebin: x%d",
    fImpl->session->Histogram()->GetName(), fImpl->session->Canvas()->GetName(),
    fImpl->session->Pad()->GetName(), state.rebinFactor));
  for(int index = 0; index < 3; ++index) {
    const bool active = static_cast<int>(state.interaction) == index;
    fImpl->tools[index]->SetState(active ? kButtonDown : kButtonUp, false);
  }
  const bool current = state.hasResult && state.resultRevision == state.revision && !fImpl->dirty;
  fImpl->resultStatus->SetText(!state.hasResult ? "Not fitted" : !current ?
    "Previous result: input changed or has unapplied edits. Refit required." :
    state.result.status == 0 ? "Result matches the current input." : "Fit failed or did not converge.");
  std::ostringstream output;
  output << std::setprecision(10);
  if(state.hasResult) {
    output << "Model: " << state.result.model << "\nStatus: " << state.result.status
      << "\nFit range: [" << state.fittedRequest.fitLow << ", " << state.fittedRequest.fitHigh << "]"
      << "\nRebin at fit: x" << state.fittedRebinFactor
      << "\nChi2 / NDF: " << state.result.chi2 << " / " << state.result.ndf
      << "\nReduced chi2: " << state.result.reducedChi2 << "\n";
    for(std::size_t index = 0; index < state.result.peaks.size(); ++index) {
      const auto& peak = state.result.peaks[index];
      const auto id = index < state.fittedRequest.peaks.size() ? state.fittedRequest.peaks[index].id : index + 1;
      output << "\nPeak " << id << "\n  Centroid: " << peak.centroid << " +/- " << peak.centroidError
        << "\n  FWHM: " << peak.fwhm << " +/- " << peak.fwhmError
        << "\n  Height: " << peak.height << " +/- " << peak.heightError
        << "\n  Area: " << peak.area << " +/- " << peak.areaError << "\n";
    }
    output << "\nTotal area: " << state.result.area << " +/- " << state.result.areaError << "\n";
  } else {
    output << "No fit result. Select peaks and choose Fit.\n";
  }
  fImpl->results->LoadBuffer(output.str().c_str());
  if(!fImpl->dirty) {
    fImpl->rangeLow->SetNumber(request.fitLow);
    fImpl->rangeHigh->SetNumber(request.fitHigh);
    fImpl->peakList->RemoveAll();
    const PhotoPeakSeed* selected = nullptr;
    for(const auto& peak : request.peaks) {
      fImpl->peakList->AddEntry(TString::Format("Peak %llu    Centroid %.8g",
        static_cast<unsigned long long>(peak.id), peak.centroid.value), static_cast<int>(peak.id));
      if(peak.id == state.selectedPeak)
        selected = &peak;
    }
    fImpl->editedPeak = selected ? selected->id : 0;
    if(selected) {
      fImpl->peakList->Select(static_cast<int>(selected->id));
      fImpl->peakList->SetTopEntry(static_cast<int>(selected->id));
      fImpl->selected->SetText(TString::Format("Selected peak: %llu (%zu total)",
        static_cast<unsigned long long>(selected->id), request.peaks.size()));
      fImpl->peakRows[0]->Set(selected->centroid);
      fImpl->peakRows[1]->Set(selected->height);
      fImpl->peakRows[2]->Set(selected->fwhm);
    } else {
      fImpl->selected->SetText("No peak selected");
      for(auto& row : fImpl->peakRows)
        row->Set(PhotoPeakParameterControl{});
    }
    const bool multiple = request.peaks.size() > 1;
    const bool lowStat = request.config.mode == PhotoPeakFitMode::LowStat;
    fImpl->peakRows[0]->SetEnabled(selected && (!multiple || !request.config.relativePosition || selected->id == request.peaks.front().id));
    fImpl->peakRows[1]->SetEnabled(selected != nullptr);
    fImpl->peakRows[2]->SetEnabled(selected && (!multiple || !request.config.relativeFwhm));
    fImpl->mode->Select(static_cast<int>(request.config.mode), false);
    fImpl->rootOptions->SetText(request.config.rootOptions.c_str(), false);
    fImpl->relativePosition->SetState(request.config.relativePosition ? kButtonDown : kButtonUp, false);
    fImpl->relativeFwhm->SetState(request.config.relativeFwhm ? kButtonDown : kButtonUp, false);
    fImpl->relativePosition->SetEnabled(multiple);
    fImpl->relativeFwhm->SetEnabled(multiple);
    const int parameterMap[] = {kPhotoPeakR, kPhotoPeakBeta, kPhotoPeakStep, kPhotoPeakA, kPhotoPeakB, kPhotoPeakC};
    for(int index = 0; index < 6; ++index) {
      fImpl->globals[index]->Set(request.config.global[parameterMap[index]]);
      const bool disabled = lowStat && (index < 3 || index == 5);
      const bool noTail = index == 1 && request.config.global[kPhotoPeakR].mode == PhotoPeakParameterMode::Fixed &&
        request.config.global[kPhotoPeakR].value == 0.0;
      fImpl->globals[index]->SetEnabled(!disabled && !noTail);
    }
    fImpl->globals[6]->Set(request.config.widthScale);
    fImpl->globals[6]->SetEnabled(multiple && request.config.relativeFwhm);
    fImpl->activeParameters->SetText(lowStat ? "LowStat fixes R, BETA, STEP and C at zero for the fit. Stored values are retained." :
      "Relative position uses the first peak as reference. Relative FWHM uses stored widths and WSCALE.");
    fImpl->background->Select(static_cast<int>(request.config.background), false);
    fImpl->backgroundIterations->SetNumber(request.config.backgroundIterations);
    fImpl->backgroundLow->SetNumber(request.config.backgroundLow);
    fImpl->backgroundHigh->SetNumber(request.config.backgroundHigh);
    SetBackgroundOptions(fImpl->backgroundOptionMenu.get(), request.config.backgroundOptions);
    fImpl->backgroundOptions->SetText(TString::Format("Options (%zu)", std::count_if(std::begin(kBackgroundOptionList),
      std::end(kBackgroundOptionList), [&](const auto& option) { return fImpl->backgroundOptionMenu->IsEntryChecked(option.id); })));
    const bool backgroundEnabled = request.config.background != PhotoPeakBackgroundMode::None;
    fImpl->backgroundIterations->SetEnabled(backgroundEnabled);
    fImpl->backgroundOptions->SetEnabled(backgroundEnabled);
    fImpl->backgroundLow->SetEnabled(request.config.background == PhotoPeakBackgroundMode::Local);
    fImpl->backgroundHigh->SetEnabled(request.config.background == PhotoPeakBackgroundMode::Local);
  }
  fImpl->refreshing = false;
  Layout();
}

// ============== PhotoPeakControlWindow::Show ==============
// Purpose: Open or raise the existing non-modal panel. Inputs: None. Outputs: Visible window.
void PhotoPeakControlWindow::Show() {
  Refresh();
  MapSubwindows();
  for(auto& row : fImpl->peakRows)
    row->Update();
  for(auto& row : fImpl->globals)
    row->Update();
  Resize(GetDefaultSize());
  MapWindow();
  RaiseWindow();
}

// ============== PhotoPeakControlWindow::Hide ==============
// Purpose: Hide the control panel. Inputs: None. Outputs: Unmapped window.
void PhotoPeakControlWindow::Hide() {
  UnmapWindow();
}

// ============== PhotoPeakControlWindow::DetachSession ==============
// Purpose: Ignore queued commands after session disposal.
// Inputs: None. Outputs: Detached hidden window awaiting ROOT deletion.
void PhotoPeakControlWindow::DetachSession() {
  fImpl->session = nullptr;
  Hide();
}

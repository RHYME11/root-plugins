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
#include <TGListBox.h>
#include <TGMenu.h>
#include <TGNumberEntry.h>
#include <TGTab.h>
#include <TGTextEntry.h>
#include <TString.h>
#include <TVirtualX.h>

#include <PhotoPeakFit/PhotoPeakSession.h>

#include "PhotoPeakParameterRow.h"

namespace {
enum ControlId {
  kFit = 100,
  kClean,
  kExit,
  kAddPeak,
  kApply,
  kMode,
  kBackground,
  kBackgroundOptions,
  kRelativePosition,
  kRelativeFwhm,
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
    struct PeakRows {
      PhotoPeakParameterRow* centroid;
      PhotoPeakParameterRow* height;
      PhotoPeakParameterRow* fwhm;
    };

    PhotoPeakSession* session = nullptr;
    TGNumberEntry* rangeLow = nullptr;
    TGNumberEntry* rangeHigh = nullptr;
    TGCanvas* peakCanvas = nullptr;
    TGVerticalFrame* peakContainer = nullptr;
    TGComboBox* mode = nullptr;
    TGTextEntry* rootOptions = nullptr;
    TGComboBox* background = nullptr;
    TGNumberEntry* backgroundIterations = nullptr;
    TGTextButton* backgroundOptions = nullptr;
    std::unique_ptr<TGPopupMenu> backgroundOptionMenu;
    TGNumberEntry* backgroundLow = nullptr;
    TGNumberEntry* backgroundHigh = nullptr;
    TGCheckButton* relativePosition = nullptr;
    TGCheckButton* relativeFwhm = nullptr;
    std::vector<PeakRows> peaks;
    std::array<PhotoPeakParameterRow*, 7> globals{};
};

PhotoPeakControlWindow::PhotoPeakControlWindow(PhotoPeakSession* session)
  : TGMainFrame(gClient->GetRoot(), 650, 720), fImpl(std::make_unique<Impl>()) {
  fImpl->session = session;
  SetWindowName("fit mode");
  SetCleanup(kDeepCleanup);

  auto* range = new TGHorizontalFrame(this);
  range->AddFrame(new TGLabel(range, "fit range"),
                  new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 4, 8, 4, 4));
  fImpl->rangeLow = new TGNumberEntry(range, 0.0, 12);
  fImpl->rangeHigh = new TGNumberEntry(range, 0.0, 12);
  range->AddFrame(fImpl->rangeLow, new TGLayoutHints(kLHintsLeft, 2, 4, 4, 4));
  range->AddFrame(fImpl->rangeHigh, new TGLayoutHints(kLHintsLeft, 2, 4, 4, 4));
  auto* apply = new TGTextButton(range, "Apply", kApply);
  apply->Associate(this);
  range->AddFrame(apply, new TGLayoutHints(kLHintsLeft, 6, 2, 4, 4));
  AddFrame(range, new TGLayoutHints(kLHintsExpandX));

  fImpl->peakCanvas = new TGCanvas(this, 625, 320);
  fImpl->peakContainer = new TGVerticalFrame(fImpl->peakCanvas->GetViewPort());
  fImpl->peakCanvas->SetContainer(fImpl->peakContainer);
  AddFrame(fImpl->peakCanvas,
           new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 4, 4, 2, 2));
  auto* addPeak = new TGTextButton(this, "Add peak", kAddPeak);
  addPeak->Associate(this);
  AddFrame(addPeak, new TGLayoutHints(kLHintsLeft, 4, 2, 2, 4));

  auto* options = new TGHorizontalFrame(this);
  options->AddFrame(new TGLabel(options, "mode"),
                    new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 4, 4, 2, 2));
  fImpl->mode = new TGComboBox(options, kMode);
  fImpl->mode->AddEntry("auto", 0);
  fImpl->mode->AddEntry("highstat", 1);
  fImpl->mode->AddEntry("lowstat", 2);
  fImpl->mode->Associate(this);
  fImpl->mode->Resize(120, 26);
  fImpl->mode->GetListBox()->Resize(120, 78);
  options->AddFrame(fImpl->mode, new TGLayoutHints(kLHintsLeft, 2, 8, 2, 2));
  fImpl->relativePosition = new TGCheckButton(options, "relativePosition",
                                               kRelativePosition);
  fImpl->relativeFwhm = new TGCheckButton(options, "relativeFwhm",
                                           kRelativeFwhm);
  fImpl->relativePosition->Associate(this);
  fImpl->relativeFwhm->Associate(this);
  options->AddFrame(fImpl->relativePosition,
                    new TGLayoutHints(kLHintsLeft, 4, 4, 2, 2));
  options->AddFrame(fImpl->relativeFwhm,
                    new TGLayoutHints(kLHintsLeft, 4, 4, 2, 2));
  AddFrame(options, new TGLayoutHints(kLHintsExpandX));

  auto* fitOptions = new TGHorizontalFrame(this);
  fitOptions->AddFrame(new TGLabel(fitOptions, "ROOT options"),
    new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 4, 4, 2, 2));
  fImpl->rootOptions = new TGTextEntry(fitOptions, "RQSN");
  fitOptions->AddFrame(fImpl->rootOptions,
    new TGLayoutHints(kLHintsExpandX, 2, 4, 2, 2));
  AddFrame(fitOptions, new TGLayoutHints(kLHintsExpandX));

  auto* backgroundGroup = new TGVerticalFrame(this);
  auto* background = new TGHorizontalFrame(backgroundGroup);
  background->AddFrame(new TGLabel(background, "background"),
    new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 4, 4, 2, 2));
  fImpl->background = new TGComboBox(background, kBackground);
  fImpl->background->AddEntry("none", 0);
  fImpl->background->AddEntry("local", 1);
  fImpl->background->AddEntry("global", 2);
  fImpl->background->Associate(this);
  fImpl->background->Resize(120, 26);
  fImpl->background->GetListBox()->Resize(120, 78);
  background->AddFrame(fImpl->background,
    new TGLayoutHints(kLHintsLeft, 2, 6, 2, 2));
  background->AddFrame(new TGLabel(background, "iterations"),
    new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 2, 2, 2, 2));
  fImpl->backgroundIterations = new TGNumberEntry(background, 20, 5);
  background->AddFrame(fImpl->backgroundIterations,
    new TGLayoutHints(kLHintsLeft, 2, 4, 2, 2));
  fImpl->backgroundOptions = new TGTextButton(
    background, "options (3)  \342\226\276", kBackgroundOptions);
  fImpl->backgroundOptions->Associate(this);
  background->AddFrame(fImpl->backgroundOptions,
    new TGLayoutHints(kLHintsLeft, 2, 4, 2, 2));
  backgroundGroup->AddFrame(background, new TGLayoutHints(kLHintsExpandX));

  auto* localRange = new TGHorizontalFrame(backgroundGroup);
  localRange->AddFrame(new TGLabel(localRange, "local range"),
    new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 2, 2, 2, 2));
  fImpl->backgroundLow = new TGNumberEntry(localRange, 0.0, 9);
  fImpl->backgroundHigh = new TGNumberEntry(localRange, 0.0, 9);
  localRange->AddFrame(fImpl->backgroundLow,
    new TGLayoutHints(kLHintsLeft, 2, 2, 2, 2));
  localRange->AddFrame(fImpl->backgroundHigh,
    new TGLayoutHints(kLHintsLeft, 2, 4, 2, 2));
  backgroundGroup->AddFrame(localRange, new TGLayoutHints(kLHintsExpandX));
  AddFrame(backgroundGroup, new TGLayoutHints(kLHintsExpandX));

  fImpl->backgroundOptionMenu = std::make_unique<TGPopupMenu>(gClient->GetRoot());
  for(const auto& option : kBackgroundOptionList)
    fImpl->backgroundOptionMenu->AddEntry(option.token, option.id);
  fImpl->backgroundOptionMenu->Associate(this);
  SetBackgroundOptions(fImpl->backgroundOptionMenu.get(),
    "BackDecreasingWindow BackOrder2 nosmoothing");

  auto* globalFrame = new TGVerticalFrame(this);
  const char* names[7] = {"R", "BETA", "STEP", "A", "B", "C", "WSCALE"};
  for(int index = 0; index < 7; ++index)
    fImpl->globals[index] = new PhotoPeakParameterRow(
      globalFrame, names[index], 300 + index * 10, this);
  AddFrame(globalFrame, new TGLayoutHints(kLHintsExpandX, 4, 4, 2, 2));

  auto* buttons = new TGHorizontalFrame(this);
  for(const auto& item : std::vector<std::pair<const char*, int>>{
        {"Fit", kFit}, {"Clean", kClean}, {"Exit mode", kExit}}) {
    auto* button = new TGTextButton(buttons, item.first, item.second);
    button->Associate(this);
    buttons->AddFrame(button, new TGLayoutHints(kLHintsLeft, 4, 4, 6, 6));
  }
  AddFrame(buttons, new TGLayoutHints(kLHintsCenterX));
  Refresh();
}

PhotoPeakControlWindow::~PhotoPeakControlWindow() = default;

Bool_t PhotoPeakControlWindow::ProcessMessage(Long_t message, Long_t parameter1,
                                               Long_t) {
  if(GET_MSG(message) != kC_COMMAND)
    return TGMainFrame::ProcessMessage(message, parameter1, 0);
  auto pull = [&]() {
    PhotoPeakFitRequest request = fImpl->session->State().request;
    request.fitLow = fImpl->rangeLow->GetNumber();
    request.fitHigh = fImpl->rangeHigh->GetNumber();
    request.peaks.clear();
    for(auto& rows : fImpl->peaks) {
      PhotoPeakSeed seed;
      seed.centroid = rows.centroid->Get();
      seed.height = rows.height->Get();
      seed.fwhm = rows.fwhm->Get();
      request.peaks.push_back(seed);
    }
    request.config.mode = static_cast<PhotoPeakFitMode>(fImpl->mode->GetSelected());
    request.config.rootOptions = fImpl->rootOptions->GetText();
    request.config.background = static_cast<PhotoPeakBackgroundMode>(
      fImpl->background->GetSelected());
    request.config.backgroundIterations = static_cast<int>(
      fImpl->backgroundIterations->GetNumber());
    request.config.backgroundOptions = BackgroundOptionsFromMenu(
      fImpl->backgroundOptionMenu.get());
    request.config.backgroundLow = fImpl->backgroundLow->GetNumber();
    request.config.backgroundHigh = fImpl->backgroundHigh->GetNumber();
    request.config.relativePosition = fImpl->relativePosition->IsOn();
    request.config.relativeFwhm = fImpl->relativeFwhm->IsOn();
    const int parameterMap[6] = {kPhotoPeakR, kPhotoPeakBeta, kPhotoPeakStep,
                                 kPhotoPeakA, kPhotoPeakB, kPhotoPeakC};
    for(int index = 0; index < 6; ++index)
      request.config.global[parameterMap[index]] = fImpl->globals[index]->Get();
    request.config.widthScale = fImpl->globals[6]->Get();
    fImpl->session->ReplaceRequest(request);
  };
  switch(parameter1) {
    case kFit: pull(); fImpl->session->Fit(); return kTRUE;
    case kClean: fImpl->session->Clean(); return kTRUE;
    case kExit: fImpl->session->ExitMode(); return kTRUE;
    case kApply: pull(); return kTRUE;
    case kAddPeak:
      pull();
      fImpl->session->AddPeak(0.5 * (fImpl->rangeLow->GetNumber() +
                                     fImpl->rangeHigh->GetNumber()));
      Refresh();
      return kTRUE;
    case kBackgroundOptions: {
      Int_t x = 0;
      Int_t y = 0;
      Window_t child = 0;
      gVirtualX->TranslateCoordinates(fImpl->backgroundOptions->GetId(),
        gClient->GetRoot()->GetId(), 0,
        fImpl->backgroundOptions->GetHeight(), x, y, child);
      fImpl->backgroundOptionMenu->PlaceMenu(x, y, false, true);
      return kTRUE;
    }
    case kMode:
    case kBackground:
    case kRelativePosition:
    case kRelativeFwhm:
      pull();
      return kTRUE;
    default:
      if(parameter1 >= kBackDecreasingWindow && parameter1 <= kBackCompton) {
        ToggleBackgroundOption(fImpl->backgroundOptionMenu.get(),
                               static_cast<int>(parameter1));
        pull();
        return kTRUE;
      }
      for(auto& row : fImpl->globals)
        row->HandleToggle(static_cast<int>(parameter1));
      for(auto& peak : fImpl->peaks) {
        peak.centroid->HandleToggle(static_cast<int>(parameter1));
        peak.height->HandleToggle(static_cast<int>(parameter1));
        peak.fwhm->HandleToggle(static_cast<int>(parameter1));
      }
      return kTRUE;
  }
}

void PhotoPeakControlWindow::CloseWindow() {
  if(fImpl->session)
    fImpl->session->ExitMode();
}

void PhotoPeakControlWindow::Refresh() {
  if(!fImpl->session)
    return;
  const auto& request = fImpl->session->State().request;
  fImpl->rangeLow->SetNumber(request.fitLow);
  fImpl->rangeHigh->SetNumber(request.fitHigh);
  for(auto& rows : fImpl->peaks) {
    delete rows.centroid;
    delete rows.height;
    delete rows.fwhm;
  }
  fImpl->peaks.clear();
  fImpl->peakContainer->Cleanup();
  int identifier = 1000;
  for(std::size_t index = 0; index < request.peaks.size(); ++index) {
    auto* group = new TGGroupFrame(fImpl->peakContainer,
      (std::string("initial peak ") + std::to_string(index + 1)).c_str());
    Impl::PeakRows rows;
    rows.centroid = new PhotoPeakParameterRow(group, "centroid", identifier, this);
    rows.height = new PhotoPeakParameterRow(group, "height", identifier + 10, this);
    rows.fwhm = new PhotoPeakParameterRow(group, "FWHM", identifier + 20, this);
    rows.centroid->Set(request.peaks[index].centroid);
    rows.height->Set(request.peaks[index].height);
    rows.fwhm->Set(request.peaks[index].fwhm);
    rows.fwhm->SetEnabled(!request.config.relativeFwhm || request.peaks.size() < 2);
    fImpl->peakContainer->AddFrame(group, new TGLayoutHints(kLHintsExpandX));
    fImpl->peaks.push_back(rows);
    identifier += 40;
  }
  fImpl->mode->Select(static_cast<int>(request.config.mode), false);
  fImpl->rootOptions->SetText(request.config.rootOptions.c_str());
  fImpl->background->Select(static_cast<int>(request.config.background), false);
  fImpl->backgroundIterations->SetNumber(request.config.backgroundIterations);
  SetBackgroundOptions(fImpl->backgroundOptionMenu.get(),
                       request.config.backgroundOptions);
  fImpl->backgroundOptions->SetText(TString::Format("options (%zu)  \342\226\276",
    std::count_if(std::begin(kBackgroundOptionList),
                  std::end(kBackgroundOptionList), [&](const auto& option) {
      return fImpl->backgroundOptionMenu->IsEntryChecked(option.id);
    })).Data());
  fImpl->backgroundLow->SetNumber(request.config.backgroundLow);
  fImpl->backgroundHigh->SetNumber(request.config.backgroundHigh);
  const bool backgroundEnabled =
    request.config.background != PhotoPeakBackgroundMode::None;
  const bool localBackground =
    request.config.background == PhotoPeakBackgroundMode::Local;
  fImpl->backgroundIterations->SetState(backgroundEnabled);
  fImpl->backgroundOptions->SetEnabled(backgroundEnabled);
  fImpl->backgroundLow->SetState(localBackground);
  fImpl->backgroundHigh->SetState(localBackground);
  const bool multiple = request.peaks.size() > 1;
  fImpl->relativePosition->SetEnabled(multiple);
  fImpl->relativeFwhm->SetEnabled(multiple);
  fImpl->relativePosition->SetState(request.config.relativePosition ?
                                    kButtonDown : kButtonUp, false);
  fImpl->relativeFwhm->SetState(request.config.relativeFwhm ?
                                kButtonDown : kButtonUp, false);
  const int parameterMap[6] = {kPhotoPeakR, kPhotoPeakBeta, kPhotoPeakStep,
                               kPhotoPeakA, kPhotoPeakB, kPhotoPeakC};
  for(int index = 0; index < 6; ++index)
    fImpl->globals[index]->Set(request.config.global[parameterMap[index]]);
  const bool lowStat = request.config.mode == PhotoPeakFitMode::LowStat;
  for(int index = 0; index < 6; ++index)
    fImpl->globals[index]->SetEnabled(!lowStat);
  const auto& rControl = request.config.global[kPhotoPeakR];
  if(rControl.mode == PhotoPeakParameterMode::Fixed && rControl.value == 0.0)
    fImpl->globals[1]->SetEnabled(false);
  fImpl->globals[6]->Set(request.config.widthScale);
  fImpl->globals[6]->SetEnabled(!lowStat && multiple &&
                                request.config.relativeFwhm);
  MapSubwindows();
  Layout();
  fImpl->peakContainer->Layout();
}

void PhotoPeakControlWindow::Show() {
  MapSubwindows();
  Resize(GetDefaultSize());
  MapWindow();
  RaiseWindow();
}

void PhotoPeakControlWindow::Hide() {
  UnmapWindow();
}

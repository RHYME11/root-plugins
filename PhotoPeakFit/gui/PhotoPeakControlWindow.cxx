#include <PhotoPeakFit/PhotoPeakControlWindow.h>

#include <array>
#include <vector>

#include <TGButton.h>
#include <TGCanvas.h>
#include <TGComboBox.h>
#include <TGLabel.h>
#include <TGNumberEntry.h>
#include <TGTab.h>
#include <TGTextEntry.h>

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
  kRelativePosition,
  kRelativeFwhm
};
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
    TGTextEntry* backgroundOptions = nullptr;
    TGNumberEntry* backgroundLow = nullptr;
    TGNumberEntry* backgroundHigh = nullptr;
    TGCheckButton* relativePosition = nullptr;
    TGCheckButton* relativeFwhm = nullptr;
    std::vector<PeakRows> peaks;
    std::array<PhotoPeakParameterRow*, 7> globals{};
};

PhotoPeakControlWindow::PhotoPeakControlWindow(PhotoPeakSession* session)
  : TGMainFrame(gClient->GetRoot(), 760, 720), fImpl(std::make_unique<Impl>()) {
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

  fImpl->peakCanvas = new TGCanvas(this, 735, 320);
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

  auto* background = new TGHorizontalFrame(this);
  background->AddFrame(new TGLabel(background, "background"),
    new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 4, 4, 2, 2));
  fImpl->background = new TGComboBox(background, kBackground);
  fImpl->background->AddEntry("none", 0);
  fImpl->background->AddEntry("local", 1);
  fImpl->background->AddEntry("global", 2);
  fImpl->background->Associate(this);
  background->AddFrame(fImpl->background,
    new TGLayoutHints(kLHintsLeft, 2, 6, 2, 2));
  background->AddFrame(new TGLabel(background, "iterations"),
    new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 2, 2, 2, 2));
  fImpl->backgroundIterations = new TGNumberEntry(background, 20, 5);
  background->AddFrame(fImpl->backgroundIterations,
    new TGLayoutHints(kLHintsLeft, 2, 4, 2, 2));
  fImpl->backgroundOptions = new TGTextEntry(background);
  background->AddFrame(fImpl->backgroundOptions,
    new TGLayoutHints(kLHintsExpandX, 2, 4, 2, 2));
  background->AddFrame(new TGLabel(background, "local range"),
    new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 2, 2, 2, 2));
  fImpl->backgroundLow = new TGNumberEntry(background, 0.0, 8);
  fImpl->backgroundHigh = new TGNumberEntry(background, 0.0, 8);
  background->AddFrame(fImpl->backgroundLow,
    new TGLayoutHints(kLHintsLeft, 2, 2, 2, 2));
  background->AddFrame(fImpl->backgroundHigh,
    new TGLayoutHints(kLHintsLeft, 2, 4, 2, 2));
  AddFrame(background, new TGLayoutHints(kLHintsExpandX));

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
    request.config.backgroundOptions = fImpl->backgroundOptions->GetText();
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
    case kMode:
    case kBackground:
    case kRelativePosition:
    case kRelativeFwhm:
      pull();
      return kTRUE;
    default:
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
  fImpl->backgroundOptions->SetText(request.config.backgroundOptions.c_str());
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

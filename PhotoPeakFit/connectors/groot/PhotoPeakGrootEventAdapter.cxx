#include "PhotoPeakGrootEventAdapter.h"

#include <Buttons.h>
#include <TFrame.h>
#include <TH1.h>
#include <TObject.h>

#include <Plugin/GPluginHost.h>
#include <Plugin/GPluginEvent.h>

#include <PhotoPeakFit/PhotoPeakSession.h>

#include "PhotoPeakGrootContextResolver.h"

namespace {

constexpr UInt_t kPhotoPeakMarkerObject = 0x50500001;

// ============== ShouldForwardPhotoPeakEvent ==============
// Purpose: Keep ROOT-native regions independent from PhotoPeak interaction.
// Inputs: ROOT-processed event and locked histogram.
// Outputs: True only for PhotoPeak keys, plot clicks, and active drags.
bool ShouldForwardPhotoPeakEvent(const GPluginEvent& event, TH1* histogram) {
  switch(event.type) {
    case kKeyPress:
    case kArrowKeyPress:
    case kButton1Motion:
    case kButton1ShiftMotion:
    case kButton1Up:
      return true;
    case kButton1Down:
    case kButton1Shift:
      return event.context.selected == histogram ||
        dynamic_cast<TFrame*>(event.context.selected) ||
        (event.context.selected &&
         event.context.selected->GetUniqueID() == kPhotoPeakMarkerObject);
    default:
      return false;
  }
}

} // namespace

PhotoPeakGrootEventAdapter::PhotoPeakGrootEventAdapter(
  GPluginHost* host, TCanvas* canvas, TVirtualPad* pad, TH1* histogram)
  : fHost(host),
    fHistogram(histogram),
    fSession(std::make_unique<PhotoPeakSession>(canvas, pad, histogram)) {
  fSession->SetExitCallback([this]() {
    fHistogram = nullptr;
    if(fHost)
      fHost->DeactivateSession(this);
  });
}

PhotoPeakGrootEventAdapter::~PhotoPeakGrootEventAdapter() {
  Close();
}

const char* PhotoPeakGrootEventAdapter::SessionId() const {
  return "photopeakfit.session";
}

void PhotoPeakGrootEventAdapter::ObserveEvent(const GPluginEvent& event) {
  if(!IsPhotoPeakTargetValid(event.context.pad, fHistogram)) {
    if(fHost)
      fHost->SetStatusMessage("PhotoPeak target was removed; fit mode closed");
    if(fSession)
      fSession->Abandon();
    fHistogram = nullptr;
    if(fHost)
      fHost->DeactivateSession(this);
    return;
  }
  if(!ShouldForwardPhotoPeakEvent(event, fHistogram))
    return;
  PhotoPeakInputEvent input;
  input.type = event.type;
  input.code = event.code;
  input.px = event.px;
  input.x = event.x;
  fSession->HandleEvent(input);
}

void PhotoPeakGrootEventAdapter::Close() {
  if(fSession)
    fSession->Abandon();
  fHistogram = nullptr;
}

// ============== PhotoPeakGrootEventAdapter::ExitMode ==============
// Purpose: Exercise the user Exit-mode path with complete session discard.
// Inputs: None.
// Outputs: Inactive abandoned session and host deactivation callback.
void PhotoPeakGrootEventAdapter::ExitMode() {
  if(fSession)
    fSession->ExitMode();
}

// ============== PhotoPeakGrootEventAdapter::CleanArtifacts ==============
// Purpose: Delete PhotoPeak drawings associated with this adapter.
// Inputs: None.
// Outputs: Markers, fit curves, legend, labels, and background removed.
void PhotoPeakGrootEventAdapter::CleanArtifacts() {
  if(fSession)
    fSession->Clean();
}

void PhotoPeakGrootEventAdapter::RaiseWindow() {
  if(fSession)
    fSession->RaiseWindow();
}

TVirtualPad* PhotoPeakGrootEventAdapter::Pad() const {
  return fSession ? fSession->Pad() : nullptr;
}

TH1* PhotoPeakGrootEventAdapter::Target() const {
  return fHistogram;
}

bool PhotoPeakGrootEventAdapter::IsClosed() const {
  return !fSession || fSession->IsClosed();
}

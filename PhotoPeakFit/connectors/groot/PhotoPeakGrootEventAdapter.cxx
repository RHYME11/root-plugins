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
  input.state = event.state;
  input.px = event.px;
  input.py = event.py;
  input.x = event.x;
  input.y = event.y;
  fSession->HandleEvent(input);
}

void PhotoPeakGrootEventAdapter::Close() {
  if(fSession)
    fSession->Abandon();
  fHistogram = nullptr;
}

// ============== PhotoPeakGrootEventAdapter::Suspend ==============
// Purpose: Exercise the user Exit-mode path while retaining session state.
// Inputs: None.
// Outputs: Inactive resumable session and host deactivation callback.
void PhotoPeakGrootEventAdapter::Suspend() {
  if(fSession)
    fSession->ExitMode();
}

// ============== PhotoPeakGrootEventAdapter::Resume ==============
// Purpose: Reactivate the retained PhotoPeak session state and controls.
// Inputs: None.
// Outputs: Active common session.
void PhotoPeakGrootEventAdapter::Resume() {
  if(fSession)
    fSession->Resume();
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

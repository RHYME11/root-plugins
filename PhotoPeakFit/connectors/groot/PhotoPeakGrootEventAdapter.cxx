#include "PhotoPeakGrootEventAdapter.h"

#include <Plugin/GPluginHost.h>

#include <PhotoPeakFit/PhotoPeakSession.h>

PhotoPeakGrootEventAdapter::PhotoPeakGrootEventAdapter(
  GPluginHost* host, TCanvas* canvas, TVirtualPad* pad, TH1* histogram)
  : fHost(host),
    fSession(std::make_unique<PhotoPeakSession>(canvas, pad, histogram)) {
  fSession->SetExitCallback([this]() {
    if(fHost)
      fHost->DeactivateSession(this);
  });
}

PhotoPeakGrootEventAdapter::~PhotoPeakGrootEventAdapter() {
  Close();
}

bool PhotoPeakGrootEventAdapter::HandleEvent(const GPluginEvent& event) {
  PhotoPeakInputEvent input;
  input.type = event.type;
  input.code = event.code;
  input.state = event.state;
  input.px = event.px;
  input.py = event.py;
  input.x = event.x;
  input.y = event.y;
  return fSession->HandleEvent(input);
}

void PhotoPeakGrootEventAdapter::Close() {
  if(fSession)
    fSession->Close();
}

void PhotoPeakGrootEventAdapter::RaiseWindow() {
  if(fSession)
    fSession->RaiseWindow();
}

TVirtualPad* PhotoPeakGrootEventAdapter::Pad() const {
  return fSession ? fSession->Pad() : nullptr;
}

bool PhotoPeakGrootEventAdapter::IsClosed() const {
  return !fSession || fSession->IsClosed();
}

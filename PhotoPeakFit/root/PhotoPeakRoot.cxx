#include <PhotoPeakFit/PhotoPeakRoot.h>
#include <PhotoPeakFit/PhotoPeakSession.h>

#include <PhotoPeakFit/detail/PhotoPeakRootController.h>
#include "PhotoPeakTarget.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <Buttons.h>
#include <KeySymbols.h>
#include <TCanvas.h>
#include <TFrame.h>
#include <TGClient.h>
#include <TH1.h>
#include <TList.h>
#include <TQObject.h>
#include <TROOT.h>
#include <TTimer.h>
#include <TVirtualPad.h>
#include <TVirtualX.h>

namespace {

constexpr UInt_t kPhotoPeakMarkerObject = 0x50500001;
constexpr const char* kController = "PhotoPeakRootController";
constexpr const char* kCanvasEvent = "ProcessedEvent(Int_t,Int_t,Int_t,TObject*)";
constexpr const char* kGuiEvent = "ProcessedEvent(Event_t*,Window_t)";

// ============== CanvasAlive ==============
// Purpose: Test canvas identity without dereferencing a possibly deleted pointer.
// Inputs: Borrowed canvas.
// Outputs: True while registered with ROOT.
bool CanvasAlive(TCanvas* canvas) {
  return gROOT && gROOT->GetListOfCanvases() &&
    gROOT->GetListOfCanvases()->FindObject(canvas);
}

// ============== IsPlotObject ==============
// Purpose: Preserve native axis and unrelated object interaction.
// Inputs: ROOT selection and session target.
// Outputs: True for the histogram, frame, or PhotoPeak marker.
bool IsPlotObject(TObject* selected, TH1* histogram) {
  return selected && (selected == histogram || dynamic_cast<TFrame*>(selected) ||
    selected->GetUniqueID() == kPhotoPeakMarkerObject);
}

class SessionTimer : public TTimer {
public:
  // ============== SessionTimer::SessionTimer ==============
  // Purpose: Create an inactive synchronous lifecycle timer.
  // Inputs: Owning controller.
  // Outputs: Timer ready for session activation.
  explicit SessionTimer(PhotoPeakRootController* owner)
    : TTimer(100, kTRUE), fOwner(owner) {}

  // ============== SessionTimer::Notify ==============
  // Purpose: Detect removed drawings even when no canvas event follows.
  // Inputs: Synchronous ROOT timer tick.
  // Outputs: Validated sessions and renewed timer deadline.
  Bool_t Notify() override {
    Reset();
    fOwner->Poll();
    return kTRUE;
  }

private:
  PhotoPeakRootController* fOwner;
};

} // namespace

class PhotoPeakRootController::Impl {
public:
  // ============== Impl::Impl ==============
  // Purpose: Initialize controller-owned state without activating event hooks.
  // Inputs: Owning controller.
  // Outputs: Empty session registry and inactive timer.
  explicit Impl(PhotoPeakRootController* owner) : timer(owner) {}

  // ============== Impl::Find ==============
  // Purpose: Find one active pad session.
  // Inputs: Pad identity.
  // Outputs: Borrowed session or null.
  PhotoPeakSession* Find(TVirtualPad* pad) {
    for(const auto& session : sessions) {
      if(!session->IsClosed() && session->Pad() == pad)
        return session.get();
    }
    return nullptr;
  }

  std::vector<std::unique_ptr<PhotoPeakSession>> sessions;
  std::set<TCanvas*> canvases;
  SessionTimer timer;
  std::string error;
  bool guiConnected = false;
  TCanvas* arrowCanvas = nullptr;
  UInt_t arrowKey = 0;
  TCanvas* escapeCanvas = nullptr;
};

// ============== PhotoPeakRootController::Instance ==============
// Purpose: Own sessions centrally on ROOT's main event thread.
// Inputs: None.
// Outputs: Process-local controller.
PhotoPeakRootController& PhotoPeakRootController::Instance() {
  static PhotoPeakRootController controller;
  return controller;
}

// ============== PhotoPeakRootController::PhotoPeakRootController ==============
// Purpose: Register target deletion observation.
// Inputs: None.
// Outputs: Inactive controller without canvas or GUI hooks.
PhotoPeakRootController::PhotoPeakRootController() : fImpl(new Impl(this)) {
  gROOT->GetListOfCleanups()->Add(this);
}

// ============== PhotoPeakRootController::~PhotoPeakRootController ==============
// Purpose: Release owned sessions before ROOT shutdown completes.
// Inputs: None.
// Outputs: Removed observer and disconnected event hooks.
PhotoPeakRootController::~PhotoPeakRootController() {
  StopAll();
  if(gROOT && gROOT->GetListOfCleanups())
    gROOT->GetListOfCleanups()->Remove(this);
}

// ============== PhotoPeakRootController::Start ==============
// Purpose: Resolve a target and activate an independent ROOT fit session.
// Inputs: Optional histogram and pad.
// Outputs: Success or a diagnostic; existing sessions are raised.
bool PhotoPeakRootController::Start(TH1* histogram, TVirtualPad* pad) {
  fImpl->error.clear();
  Poll();
  fImpl->sessions.erase(std::remove_if(fImpl->sessions.begin(), fImpl->sessions.end(),
    [](const std::unique_ptr<PhotoPeakSession>& session) {
      return session->IsClosed();
    }), fImpl->sessions.end());
  if(!pad)
    pad = gPad;
  TCanvas* canvas = pad ? pad->GetCanvas() : nullptr;
  if(!IsPhotoPeakPadAlive(canvas, pad)) {
    fImpl->error = "select a live canvas pad first";
    return false;
  }
  if(canvas->IsWeb()) {
    fImpl->error = "PhotoPeakFit requires a native ROOT TCanvas";
    return false;
  }
  if(auto* active = fImpl->Find(pad)) {
    if(histogram && histogram != active->Histogram()) {
      fImpl->error = "this pad already has a session; stop it before switching targets";
      return false;
    }
    active->RaiseWindow();
    return true;
  }
  TH1* target = ResolvePhotoPeakTarget(pad, canvas->GetSelected(), histogram,
    fImpl->error);
  if(!target)
    return false;
  for(const auto& active : fImpl->sessions) {
    if(!active->IsClosed() && active->Histogram() == target) {
      fImpl->error = "this histogram already has a fit session in another pad";
      return false;
    }
  }
  if(!fImpl->canvases.count(canvas)) {
    const bool events = canvas->Connect(kCanvasEvent, kController, this,
      "OnEvent(Int_t,Int_t,Int_t,TObject*)");
    const bool closed = canvas->Connect("Closed()", kController, this,
      "OnCanvasClosed()");
    if(!events || !closed) {
      canvas->Disconnect(nullptr, this);
      fImpl->error = "cannot connect ROOT canvas signals";
      return false;
    }
    fImpl->canvases.insert(canvas);
  }
  target->SetBit(TObject::kMustCleanup);
  fImpl->sessions.push_back(std::make_unique<PhotoPeakSession>(canvas, pad, target));
  if(!gROOT->IsBatch() && gClient && !fImpl->guiConnected)
    fImpl->guiConnected = gClient->Connect(kGuiEvent, kController, this,
      "OnGuiEvent(Event_t*,Window_t)");
  fImpl->timer.TurnOn();
  return true;
}

// ============== PhotoPeakRootController::Clean ==============
// Purpose: Clear one active fit session through the public API.
// Inputs: Optional pad.
// Outputs: Success or an inactive-session diagnostic.
bool PhotoPeakRootController::Clean(TVirtualPad* pad) {
  fImpl->error.clear();
  Poll();
  auto* session = fImpl->Find(pad ? pad : gPad);
  if(!session) {
    fImpl->error = "no active PhotoPeakFit session in this pad";
    return false;
  }
  session->Clean();
  return true;
}

// ============== PhotoPeakRootController::Stop ==============
// Purpose: Close one session and remove unused signal connections.
// Inputs: Optional pad.
// Outputs: Success or an inactive-session diagnostic.
bool PhotoPeakRootController::Stop(TVirtualPad* pad) {
  fImpl->error.clear();
  Poll();
  auto* session = fImpl->Find(pad ? pad : gPad);
  if(!session) {
    fImpl->error = "no active PhotoPeakFit session in this pad";
    return false;
  }
  session->ExitMode();
  Poll();
  return true;
}

// ============== PhotoPeakRootController::StopAll ==============
// Purpose: Close sessions without destroying receivers on their callback stack.
// Inputs: None.
// Outputs: Released heavy resources and detached canvas and GUI hooks.
void PhotoPeakRootController::StopAll() {
  fImpl->error.clear();
  for(const auto& session : fImpl->sessions)
    session->Abandon();
  Poll();
}

// ============== PhotoPeakRootController::LastError ==============
// Purpose: Expose the current API diagnostic.
// Inputs: None.
// Outputs: Borrowed message.
const char* PhotoPeakRootController::LastError() const {
  return fImpl->error.c_str();
}

// ============== PhotoPeakRootController::OnEvent ==============
// Purpose: Observe native ROOT input after its own interaction has completed.
// Inputs: Canvas event, native pixel/key payload, and ROOT selection.
// Outputs: Normalized event delivered only to the owning live pad session.
void PhotoPeakRootController::OnEvent(Int_t event, Int_t px, Int_t py,
  TObject* selected) {
  auto* canvas = static_cast<TCanvas*>(gTQSender);
  if(!fImpl->canvases.count(canvas))
    return;
  TVirtualPad* pad = canvas->GetSelectedPad();
  if(!pad)
    pad = canvas;
  auto* session = fImpl->Find(pad);
  if(!session || !session->ValidateTarget())
    return;
  if(event == kButton1Up && fImpl->escapeCanvas == canvas) {
    fImpl->escapeCanvas = nullptr;
    session->SetInteraction(PhotoPeakInteraction::Browse);
  }
  switch(event) {
    case kButton1Down:
    case kButton1Shift:
      if(!IsPlotObject(selected, session->Histogram()))
        return;
      break;
    case kButton1Motion:
    case kButton1ShiftMotion:
    case kButton1Up:
    case kKeyPress:
      break;
    case kArrowKeyPress:
      if(fImpl->arrowCanvas != canvas || !fImpl->arrowKey)
        return;
      break;
    default:
      return;
  }
  TVirtualPad::TContext context(pad);
  PhotoPeakInputEvent input;
  input.type = event;
  input.code = event == kKeyPress ? (py ? py : px) : static_cast<int>(fImpl->arrowKey);
  input.px = px;
  input.x = pad->PadtoX(pad->AbsPixeltoX(px));
  const int left = pad->XtoAbsPixel(pad->GetUxmin());
  const int right = pad->XtoAbsPixel(pad->GetUxmax());
  const int top = pad->YtoAbsPixel(pad->GetUymax());
  const int bottom = pad->YtoAbsPixel(pad->GetUymin());
  input.inPlot = px >= left && px <= right && py >= top && py <= bottom;
  session->HandleEvent(input);
  if(!session->IsClosed()) {
    pad->Modified();
    pad->Update();
  }
}

// ============== PhotoPeakRootController::OnGuiEvent ==============
// Purpose: Preserve arrow key identity omitted by native TCanvas events.
// Inputs: Native GUI event and optional modal-window identity.
// Outputs: Key identity cached only for the corresponding canvas container.
void PhotoPeakRootController::OnGuiEvent(Event_t* event, Window_t) {
  fImpl->arrowCanvas = nullptr;
  fImpl->arrowKey = 0;
  fImpl->escapeCanvas = nullptr;
  if(!event || event->fType != kGKeyPress || !gVirtualX)
    return;
  UInt_t keysym = 0;
  char text[8] = {};
  gVirtualX->LookupString(event, text, sizeof(text), keysym);
  if(keysym != kKey_Left && keysym != kKey_Right && keysym != kKey_Escape)
    return;
  for(TCanvas* canvas : fImpl->canvases) {
    if(CanvasAlive(canvas) && canvas->GetCanvasID() >= 0 &&
       gVirtualX->GetWindowID(canvas->GetCanvasID()) == event->fWindow) {
      if(keysym == kKey_Escape)
        fImpl->escapeCanvas = canvas;
      else {
        fImpl->arrowCanvas = canvas;
        fImpl->arrowKey = keysym;
      }
      return;
    }
  }
}

// ============== PhotoPeakRootController::OnCanvasClosed ==============
// Purpose: Discard sessions after ROOT closes their canvas.
// Inputs: Canvas identity from the signal sender.
// Outputs: Closed sessions without accessing destroyed pad primitives.
void PhotoPeakRootController::OnCanvasClosed() {
  auto* canvas = static_cast<TCanvas*>(gTQSender);
  for(const auto& session : fImpl->sessions) {
    if(session->Canvas() == canvas)
      session->DetachTarget();
  }
  canvas->Disconnect(nullptr, this);
  fImpl->canvases.erase(canvas);
}

// ============== PhotoPeakRootController::RecursiveRemove ==============
// Purpose: Invalidate borrowed identities immediately during ROOT deletion.
// Inputs: Object being removed by ROOT.
// Outputs: Abandoned sessions; callback receivers stay alive until a safe entry.
void PhotoPeakRootController::RecursiveRemove(TObject* object) {
  for(const auto& session : fImpl->sessions) {
    if(session->IsClosed())
      continue;
    if(object == session->Canvas() || object == session->Pad())
      session->DetachTarget();
    else if(object == session->Histogram())
      session->Abandon();
  }
}

// ============== PhotoPeakRootController::Poll ==============
// Purpose: Validate drawings and disconnect canvases without active sessions.
// Inputs: API entry or synchronous timer tick.
// Outputs: Safe inactive sessions and no unnecessary event hooks.
void PhotoPeakRootController::Poll() {
  for(const auto& session : fImpl->sessions) {
    if(!session->IsClosed())
      session->ValidateTarget();
  }
  for(auto it = fImpl->canvases.begin(); it != fImpl->canvases.end();) {
    TCanvas* canvas = *it;
    const bool active = std::any_of(fImpl->sessions.begin(), fImpl->sessions.end(),
      [canvas](const std::unique_ptr<PhotoPeakSession>& session) {
        return !session->IsClosed() && session->Canvas() == canvas;
      });
    if(active) {
      ++it;
      continue;
    }
    if(CanvasAlive(canvas))
      canvas->Disconnect(nullptr, this);
    it = fImpl->canvases.erase(it);
  }
  if(fImpl->canvases.empty()) {
    fImpl->timer.TurnOff();
    if(fImpl->guiConnected && gClient)
      gClient->Disconnect(kGuiEvent, this);
    fImpl->guiConnected = false;
    fImpl->arrowCanvas = nullptr;
    fImpl->arrowKey = 0;
  }
}

namespace PhotoPeakFit {

// ============== Start ==============
// Purpose: Delegate public startup to the ROOT controller.
// Inputs: Optional histogram and pad.
// Outputs: Startup result.
bool Start(TH1* histogram, TVirtualPad* pad) {
  return PhotoPeakRootController::Instance().Start(histogram, pad);
}

// ============== Clean ==============
// Purpose: Delegate public cleanup to the ROOT controller.
// Inputs: Optional pad.
// Outputs: Cleanup result.
bool Clean(TVirtualPad* pad) {
  return PhotoPeakRootController::Instance().Clean(pad);
}

// ============== Stop ==============
// Purpose: Delegate public session shutdown to the ROOT controller.
// Inputs: Optional pad.
// Outputs: Shutdown result.
bool Stop(TVirtualPad* pad) {
  return PhotoPeakRootController::Instance().Stop(pad);
}

// ============== StopAll ==============
// Purpose: Delegate public global shutdown to the ROOT controller.
// Inputs: None.
// Outputs: Closed sessions.
void StopAll() {
  PhotoPeakRootController::Instance().StopAll();
}

// ============== LastError ==============
// Purpose: Delegate public diagnostic access to the ROOT controller.
// Inputs: None.
// Outputs: Borrowed diagnostic.
const char* LastError() {
  return PhotoPeakRootController::Instance().LastError();
}

} // namespace PhotoPeakFit

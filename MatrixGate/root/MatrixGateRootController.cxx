#include <MatrixGate/detail/MatrixGateRootController.h>
#include <MatrixGate/MatrixGateRoot.h>

#include "MatrixGateSession.h"
#include "MatrixGateWindow.h"

#include <algorithm>
#include <string>
#include <vector>
#include <TCanvas.h>
#include <Buttons.h>
#include <KeySymbols.h>
#include <TAxis.h>
#include <TClass.h>
#include <TContextMenu.h>
#include <TVirtualX.h>
#include <TGClient.h>
#include <TH2.h>
#include <TList.h>
#include <TROOT.h>
#include <TTimer.h>

namespace MatrixGate::Detail {
namespace {

constexpr const char* kGuiEvent = "ProcessedEvent(Event_t*,Window_t)";
constexpr const char* kController = "MatrixGate::Detail::RootController";

class SessionTimer : public TTimer {
public:
  // ============== SessionTimer ==============
  // Purpose: Poll sessions only on ROOT's GUI thread. Inputs: Controller. Outputs: Inactive timer.
  explicit SessionTimer(RootController* owner) : TTimer(75, kTRUE), fOwner(owner) {}
  // ============== Notify ==============
  // Purpose: Process deferred updates and cleanup. Inputs: Timer tick. Outputs: Continued polling.
  Bool_t Notify() override { Reset(); fOwner->Poll(); return kTRUE; }
private:
  RootController* fOwner;
};

} // namespace

class RootController::Impl {
public:
  // ============== Impl ==============
  // Purpose: Own sessions and an inactive timer. Inputs: Controller. Outputs: Empty registry.
  explicit Impl(RootController* owner) : timer(owner) {}
  std::vector<std::unique_ptr<Session>> sessions;
  SessionTimer timer;
  std::string error;
  bool polling = false;
  bool guiConnected = false;
  bool modalActive = false;
  TCanvas* arrowCanvas = nullptr;
  UInt_t arrowKey = 0;
  TCanvas* rangeCanvas = nullptr;
  TCanvas* escapeCanvas = nullptr;
  TCanvas* fallbackCanvas = nullptr;
  int fallbackKey = 0;
};

// ============== Instance ==============
// Purpose: Own GUI state on ROOT's main thread. Inputs: None. Outputs: Process-local controller.
RootController& RootController::Instance() {
  static RootController controller;
  return controller;
}

// ============== RootController ==============
// Purpose: Observe deletion of borrowed matrices. Inputs: None. Outputs: Registered observer.
RootController::RootController() : fImpl(std::make_unique<Impl>(this)) {
  gROOT->GetListOfCleanups()->Add(this);
}

// ============== ~RootController ==============
// Purpose: Disconnect all owned sessions. Inputs: Runtime registry. Outputs: Removed observer.
RootController::~RootController() {
  StopAll();
  if(gROOT && gROOT->GetListOfCleanups()) gROOT->GetListOfCleanups()->Remove(this);
}

// ============== Find ==============
// Purpose: Resolve an active session by source identity. Inputs: Matrix. Outputs: Borrowed session.
Session* RootController::Find(TH2* matrix) {
  for(const auto& session : fImpl->sessions)
    if(!session->Closed() && session->Source() == matrix) return session.get();
  return nullptr;
}

// ============== Start ==============
// Purpose: Validate input and open or raise one window. Inputs: Matrix/config. Outputs: API status.
bool RootController::Start(TH2* matrix, const Config& config) {
  TVirtualPad::TContext context;
  fImpl->error.clear();
  Poll();
  if(!matrix) { fImpl->error = "Start requires a live TH2 pointer."; return false; }
  if(gROOT->IsBatch() || !gClient) {
    fImpl->error = "MatrixGate::Start requires a native ROOT GUI session; Compute supports batch mode.";
    return false;
  }
  if(auto* session = Find(matrix)) { session->Raise(); return true; }
  auto initial = Compute(*matrix, config);
  if(!initial.success) { fImpl->error = initial.message; return false; }
  matrix->SetBit(TObject::kMustCleanup);
  fImpl->sessions.push_back(std::make_unique<Session>(matrix, config, std::move(initial)));
  if(!fImpl->guiConnected)
    fImpl->guiConnected = gClient->Connect(kGuiEvent, kController, this, "OnGuiEvent(Event_t*,Window_t)");
  fImpl->timer.TurnOn();
  return true;
}

// ============== Update ==============
// Purpose: Commit and recompute a window. Inputs: Source identity. Outputs: API status/diagnostic.
bool RootController::Update(TH2* matrix) {
  fImpl->error.clear();
  auto* session = Find(matrix);
  if(!session) { fImpl->error = "No active MatrixGate window for this matrix."; return false; }
  if(fImpl->modalActive) { fImpl->error = "Close the X-axis range dialog before updating."; return false; }
  const bool ok = session->ControlWindow() ? session->ControlWindow()->Submit(true) : session->Refresh();
  if(!ok) fImpl->error = session->Status();
  return ok;
}

// ============== Stop ==============
// Purpose: Close one window. Inputs: Source identity. Outputs: Whether a session was closed.
bool RootController::Stop(TH2* matrix) {
  fImpl->error.clear();
  auto* session = Find(matrix);
  if(!session) { fImpl->error = "No active MatrixGate window for this matrix."; return false; }
  session->Close();
  Poll();
  return true;
}

// ============== StopAll ==============
// Purpose: Release every session. Inputs: None. Outputs: Deferred window destruction and no timer.
void RootController::StopAll() {
  for(const auto& session : fImpl->sessions) session->Close();
  if(fImpl->modalActive) return;
  fImpl->sessions.clear();
  fImpl->timer.TurnOff();
  if(fImpl->guiConnected && gClient) gClient->Disconnect(kGuiEvent, this);
  fImpl->guiConnected = false;
  fImpl->arrowCanvas = fImpl->rangeCanvas = fImpl->escapeCanvas = nullptr;
  fImpl->arrowKey = 0;
  fImpl->fallbackCanvas = nullptr; fImpl->fallbackKey = 0;
}

// ============== LastError ==============
// Purpose: Read the latest API diagnostic. Inputs: None. Outputs: Borrowed message.
const char* RootController::LastError() const { return fImpl->error.c_str(); }

// ============== OnCanvasEvent ==============
// Purpose: Observe input after native ROOT processing. Inputs: Canvas signal. Outputs: Owning session edit.
void RootController::OnCanvasEvent(Int_t event, Int_t px, Int_t py, TObject* selected) {
  if(fImpl->modalActive) return;
  auto* canvas = static_cast<TCanvas*>(gTQSender);
  Session* owner = nullptr;
  for(const auto& session : fImpl->sessions) {
    if(!session->Closed() && (session->TotalCanvas() == canvas || session->ResultCanvas() == canvas)) {
      owner = session.get(); break;
    }
  }
  if(!owner) return;
  if(event == kArrowKeyPress) {
    if(fImpl->arrowCanvas != canvas || !fImpl->arrowKey) return;
    py = static_cast<int>(fImpl->arrowKey);
    fImpl->arrowCanvas = nullptr; fImpl->arrowKey = 0;
  }
  if(event == kButton1Up && fImpl->escapeCanvas == canvas) {
    fImpl->escapeCanvas = nullptr;
    owner->SetBrowse(true); return;
  }
  int key = event == kKeyPress ? (py ? py : px) : 0;
  if(event == kKeyPress && fImpl->rangeCanvas == canvas && (key == kKey_e || key == kKey_E)) {
    key = kKey_E; fImpl->rangeCanvas = nullptr;
  }
  if(event == kKeyPress) {
    const bool sameKey = key == fImpl->fallbackKey || (key == kKey_e && fImpl->fallbackKey == kKey_E);
    if(fImpl->fallbackCanvas == canvas && sameKey) { fImpl->fallbackCanvas = nullptr; return; }
    HandleKey(owner, canvas, key); return;
  }
  owner->CanvasEvent(canvas, event, px, py, selected);
}

// ============== HandleKey ==============
// Purpose: Apply one key even immediately after ROOT clears its selected drawing.
// Inputs: Owning session/canvas/normalized key. Outputs: One plot-local action or native dialog.
void RootController::HandleKey(Session* owner, TCanvas* canvas, int key) {
  if(key == kKey_E && owner->Display(canvas) && !gROOT->IsBatch()) {
    auto* histogram = owner->Display(canvas);
    auto* matrix = owner->Source();
    TVirtualPad::TContext context(canvas);
    auto* menu = canvas->GetContextMenu();
    menu->SetCanvas(canvas); menu->SetPad(canvas); menu->SetObject(histogram->GetXaxis());
    // ROOT's modal dialog retains this axis pointer; postpone timer cleanup and recalculation until it closes.
    fImpl->modalActive = true;
    menu->Action(histogram->GetXaxis(), TAxis::Class()->GetMethodAny("SetRangeUser"));
    fImpl->modalActive = false;
    if(auto* active = Find(matrix)) {
      if(active->TotalCanvas() == canvas) active->SyncView();
      else active->SyncResultView();
    }
    Poll();
    return;
  }
  owner->CanvasEvent(canvas, kKeyPress, 0, key, owner->Display(canvas));
}

// ============== OnGuiEvent ==============
// Purpose: Resolve native plot keys and Shift+E only for the canvas window receiving the key.
// Inputs: Native event/window. Outputs: One matching canvas key identity; text entries remain native.
void RootController::OnGuiEvent(Event_t* event, Window_t) {
  fImpl->arrowCanvas = fImpl->rangeCanvas = fImpl->escapeCanvas = nullptr;
  fImpl->arrowKey = 0;
  fImpl->fallbackCanvas = nullptr; fImpl->fallbackKey = 0;
  if(!event || event->fType != kGKeyPress || !gVirtualX || fImpl->modalActive) return;
  UInt_t key = 0;
  char text[8] = {};
  gVirtualX->LookupString(event, text, sizeof(text), key);
  const bool range = key == kKey_E || (key == kKey_e && (event->fState & kKeyShiftMask));
  const bool viewKey = key == kKey_o || key == kKey_w || key == kKey_q;
  if(key != kKey_Left && key != kKey_Right && key != kKey_Escape && !range && !viewKey) return;
  for(const auto& session : fImpl->sessions) if(!session->Closed()) {
    for(auto* canvas : {session->TotalCanvas(), session->ResultCanvas()}) {
      if(canvas && canvas->GetCanvasID() >= 0 &&
        gVirtualX->GetWindowID(canvas->GetCanvasID()) == event->fWindow) {
        if((viewKey || range) && !canvas->GetSelected()) {
          const int normalized = range ? kKey_E : static_cast<int>(key);
          HandleKey(session.get(), canvas, normalized);
          fImpl->fallbackCanvas = canvas; fImpl->fallbackKey = normalized;
        } else if(range) fImpl->rangeCanvas = canvas;
        else if(key == kKey_Escape) fImpl->escapeCanvas = canvas;
        else if(!viewKey) { fImpl->arrowCanvas = canvas; fImpl->arrowKey = key; }
        return;
      }
    }
  }
}

// ============== RecursiveRemove ==============
// Purpose: Forget deleted inputs immediately without dereferencing them. Inputs: Removed object. Outputs: Closed session.
void RootController::RecursiveRemove(TObject* object) {
  for(const auto& session : fImpl->sessions)
    if(session && !session->Closed() && session->Source() == object) session->Close(true);
}

// ============== Poll ==============
// Purpose: Run deferred calculation and release closed sessions outside callbacks.
// Inputs: Timer/API entry. Outputs: Live sessions only, no idle polling.
void RootController::Poll() {
  if(fImpl->polling || fImpl->modalActive) return;
  fImpl->polling = true;
  for(const auto& session : fImpl->sessions) if(!session->Closed()) session->Tick();
  fImpl->sessions.erase(std::remove_if(fImpl->sessions.begin(), fImpl->sessions.end(),
    [](const auto& session) { return session->Closed(); }), fImpl->sessions.end());
  if(fImpl->sessions.empty()) {
    fImpl->timer.TurnOff();
    if(fImpl->guiConnected && gClient) gClient->Disconnect(kGuiEvent, this);
    fImpl->guiConnected = false;
    fImpl->arrowCanvas = fImpl->rangeCanvas = fImpl->escapeCanvas = nullptr;
    fImpl->arrowKey = 0;
    fImpl->fallbackCanvas = nullptr; fImpl->fallbackKey = 0;
  }
  fImpl->polling = false;
}

} // namespace MatrixGate::Detail

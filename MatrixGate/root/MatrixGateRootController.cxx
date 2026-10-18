#include <MatrixGate/detail/MatrixGateRootController.h>
#include <MatrixGate/MatrixGateRoot.h>

#include "MatrixGateSession.h"
#include "MatrixGateWindow.h"

#include <algorithm>
#include <string>
#include <vector>
#include <TCanvas.h>
#include <TGClient.h>
#include <TH2.h>
#include <TList.h>
#include <TROOT.h>
#include <TTimer.h>

namespace MatrixGate::Detail {
namespace {

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
  fImpl->timer.TurnOn();
  return true;
}

// ============== Update ==============
// Purpose: Commit and recompute a window. Inputs: Source identity. Outputs: API status/diagnostic.
bool RootController::Update(TH2* matrix) {
  fImpl->error.clear();
  auto* session = Find(matrix);
  if(!session) { fImpl->error = "No active MatrixGate window for this matrix."; return false; }
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
  fImpl->sessions.clear();
  fImpl->timer.TurnOff();
}

// ============== LastError ==============
// Purpose: Read the latest API diagnostic. Inputs: None. Outputs: Borrowed message.
const char* RootController::LastError() const { return fImpl->error.c_str(); }

// ============== OnCanvasEvent ==============
// Purpose: Observe input after native ROOT processing. Inputs: Canvas signal. Outputs: Owning session edit.
void RootController::OnCanvasEvent(Int_t event, Int_t px, Int_t py, TObject* selected) {
  auto* canvas = static_cast<TCanvas*>(gTQSender);
  for(const auto& session : fImpl->sessions) {
    if(!session->Closed() && session->TotalCanvas() == canvas) {
      session->CanvasEvent(canvas, event, px, py, selected);
      return;
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
  if(fImpl->polling) return;
  fImpl->polling = true;
  for(const auto& session : fImpl->sessions) if(!session->Closed()) session->Tick();
  fImpl->sessions.erase(std::remove_if(fImpl->sessions.begin(), fImpl->sessions.end(),
    [](const auto& session) { return session->Closed(); }), fImpl->sessions.end());
  if(fImpl->sessions.empty()) fImpl->timer.TurnOff();
  fImpl->polling = false;
}

} // namespace MatrixGate::Detail

#include <PhotoPeakFit/PhotoPeakBridge.h>
#include <PhotoPeakFit/PhotoPeakRoot.h>
#include <PhotoPeakFit/detail/PhotoPeakRootController.h>

// ============== PhotoPeakFitBridgeVersion ==============
// Purpose: Check the runtime bridge ABI. Inputs: None. Outputs: Version 1.
extern "C" int PhotoPeakFitBridgeVersion() { return 1; }
// ============== PhotoPeakFitBridgeStart ==============
// Purpose: Reuse the native fitter. Inputs: Drawn snapshot/pad. Outputs: Success.
extern "C" bool PhotoPeakFitBridgeStart(TH1* target, TVirtualPad* pad) { return PhotoPeakFit::Start(target, pad); }
// ============== PhotoPeakFitBridgeStop ==============
// Purpose: Release one snapshot session. Inputs: Pad. Outputs: Success.
extern "C" bool PhotoPeakFitBridgeStop(TVirtualPad* pad) { return PhotoPeakFit::Stop(pad); }
// ============== PhotoPeakFitBridgeRefresh ==============
// Purpose: Refresh snapshot inputs through the controller. Inputs: Target/new data/pad. Outputs: Success.
extern "C" bool PhotoPeakFitBridgeRefresh(TH1* target, const TH1* snapshot, TVirtualPad* pad) {
  return PhotoPeakRootController::Instance().RefreshSnapshot(target, snapshot, pad);
}
// ============== PhotoPeakFitBridgeInvalidate ==============
// Purpose: Mark an external snapshot stale. Inputs: Pad/reason. Outputs: Existing-session flag.
extern "C" bool PhotoPeakFitBridgeInvalidate(TVirtualPad* pad, const char* message) {
  return PhotoPeakRootController::Instance().InvalidateSource(pad, message);
}
// ============== PhotoPeakFitBridgeLastError ==============
// Purpose: Read native bridge errors. Inputs: None. Outputs: Borrowed diagnostic.
extern "C" const char* PhotoPeakFitBridgeLastError() { return PhotoPeakFit::LastError(); }

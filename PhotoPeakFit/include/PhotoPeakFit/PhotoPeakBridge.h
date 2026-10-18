#ifndef PHOTOPEAK_BRIDGE_H
#define PHOTOPEAK_BRIDGE_H

class TH1;
class TVirtualPad;

// ============== PhotoPeakFitBridgeVersion ==============
// Purpose: Identify the optional runtime snapshot bridge. Inputs: None. Outputs: ABI version 1.
extern "C" int PhotoPeakFitBridgeVersion();
// ============== PhotoPeakFitBridgeStart ==============
// Purpose: Open the native fitter for a caller-owned snapshot. Inputs: Drawn histogram/pad. Outputs: Success.
extern "C" bool PhotoPeakFitBridgeStart(TH1* target, TVirtualPad* pad);
// ============== PhotoPeakFitBridgeStop ==============
// Purpose: Close only the addressed snapshot fit. Inputs: Pad. Outputs: Whether a fit was stopped.
extern "C" bool PhotoPeakFitBridgeStop(TVirtualPad* pad);
// ============== PhotoPeakFitBridgeRefresh ==============
// Purpose: Refresh snapshot data while retaining fit inputs. Inputs: Target/new data/pad. Outputs: Success.
extern "C" bool PhotoPeakFitBridgeRefresh(TH1* target, const TH1* snapshot, TVirtualPad* pad);
// ============== PhotoPeakFitBridgeInvalidate ==============
// Purpose: Mark an unchanged snapshot obsolete. Inputs: Pad/diagnostic. Outputs: Whether its session exists.
extern "C" bool PhotoPeakFitBridgeInvalidate(TVirtualPad* pad, const char* message);
// ============== PhotoPeakFitBridgeLastError ==============
// Purpose: Read the last bridge failure. Inputs: None. Outputs: Borrowed diagnostic.
extern "C" const char* PhotoPeakFitBridgeLastError();

#endif

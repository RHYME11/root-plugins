#ifndef PHOTOPEAK_ROOT_H
#define PHOTOPEAK_ROOT_H

class TH1;
class TVirtualPad;

namespace PhotoPeakFit {

// ============== Start ==============
// Purpose: Open or raise a fit session for one drawn one-dimensional histogram.
// Inputs: Optional histogram and pad; omitted arguments use the current selection.
// Outputs: Success, or false with a diagnostic available through LastError().
bool Start(TH1* histogram = nullptr, TVirtualPad* pad = nullptr);

// ============== Clean ==============
// Purpose: Clear the active session's fit artifacts, range, and peak seeds.
// Inputs: Optional session pad; defaults to gPad.
// Outputs: True when an active session was cleaned.
bool Clean(TVirtualPad* pad = nullptr);

// ============== Stop ==============
// Purpose: Close one fit session and release its resources and event connections.
// Inputs: Optional session pad; defaults to gPad.
// Outputs: True when an active session was closed.
bool Stop(TVirtualPad* pad = nullptr);

// ============== StopAll ==============
// Purpose: Close every PhotoPeakFit session.
// Inputs: None.
// Outputs: Released session resources and event connections.
void StopAll();

// ============== LastError ==============
// Purpose: Read the most recent public API diagnostic.
// Inputs: None.
// Outputs: Message valid until the next public API call.
const char* LastError();

} // namespace PhotoPeakFit

#endif

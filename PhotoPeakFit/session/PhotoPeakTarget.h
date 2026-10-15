#ifndef PHOTOPEAK_TARGET_H
#define PHOTOPEAK_TARGET_H

#include <string>

class TCanvas;
class TH1;
class TObject;
class TVirtualPad;

bool IsPhotoPeakPadAlive(TCanvas* canvas, TVirtualPad* pad);
bool IsPhotoPeakTargetDrawn(TVirtualPad* pad, TH1* histogram);
TH1* ResolvePhotoPeakTarget(TVirtualPad* pad, TObject* selected,
  TH1* explicitTarget, std::string& error);

#endif

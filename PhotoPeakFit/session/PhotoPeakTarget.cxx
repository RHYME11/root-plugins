#include "PhotoPeakTarget.h"

#include <TCanvas.h>
#include <TH1.h>
#include <TList.h>
#include <TROOT.h>
#include <TVirtualPad.h>

namespace {

// ============== ContainsPad ==============
// Purpose: Locate a borrowed pad by identity in a live canvas hierarchy.
// Inputs: Live parent and requested pad identity.
// Outputs: True without dereferencing a missing requested pad.
bool ContainsPad(TVirtualPad* parent, TVirtualPad* requested) {
  if(parent == requested)
    return true;
  if(!parent->GetListOfPrimitives())
    return false;
  TIter next(parent->GetListOfPrimitives());
  while(TObject* object = next()) {
    auto* child = dynamic_cast<TVirtualPad*>(object);
    if(child && ContainsPad(child, requested))
      return true;
  }
  return false;
}

} // namespace

// ============== IsPhotoPeakPadAlive ==============
// Purpose: Validate borrowed canvas and pad pointers before accessing them.
// Inputs: Canvas and pad identities.
// Outputs: True only while both remain in ROOT's live canvas hierarchy.
bool IsPhotoPeakPadAlive(TCanvas* canvas, TVirtualPad* pad) {
  return canvas && pad && gROOT && gROOT->GetListOfCanvases() &&
    gROOT->GetListOfCanvases()->FindObject(canvas) && ContainsPad(canvas, pad);
}

// ============== IsPhotoPeakTargetDrawn ==============
// Purpose: Check direct target membership without dereferencing the target.
// Inputs: Live pad and histogram identity.
// Outputs: True when the histogram is still directly drawn.
bool IsPhotoPeakTargetDrawn(TVirtualPad* pad, TH1* histogram) {
  return pad && histogram && pad->GetListOfPrimitives() &&
    pad->GetListOfPrimitives()->FindObject(histogram) == histogram;
}

// ============== ResolvePhotoPeakTarget ==============
// Purpose: Select an explicit, selected, or sole drawn one-dimensional histogram.
// Inputs: Live pad, selection, optional explicit target, and error output.
// Outputs: Valid target or null with an actionable diagnostic.
TH1* ResolvePhotoPeakTarget(TVirtualPad* pad, TObject* selected,
  TH1* explicitTarget, std::string& error) {
  if(explicitTarget) {
    if(IsPhotoPeakTargetDrawn(pad, explicitTarget) &&
       explicitTarget->GetDimension() == 1)
      return explicitTarget;
    error = "the requested target must be a one-dimensional histogram drawn in the pad";
    return nullptr;
  }
  auto* selectedHistogram = dynamic_cast<TH1*>(selected);
  if(IsPhotoPeakTargetDrawn(pad, selectedHistogram) &&
     selectedHistogram->GetDimension() == 1)
    return selectedHistogram;
  TH1* target = nullptr;
  if(pad && pad->GetListOfPrimitives()) {
    TIter next(pad->GetListOfPrimitives());
    while(TObject* object = next()) {
      auto* histogram = dynamic_cast<TH1*>(object);
      if(!histogram || histogram->GetDimension() != 1)
        continue;
      if(target) {
        error = "multiple histograms are drawn; select one or pass it to Start(histogram, pad)";
        return nullptr;
      }
      target = histogram;
    }
  }
  if(!target)
    error = "draw a one-dimensional histogram in the pad first";
  return target;
}

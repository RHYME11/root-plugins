#include "PhotoPeakDrawController.h"

#include <string>
#include <vector>

#include <TH1.h>
#include <TList.h>
#include <TObject.h>
#include <TString.h>
#include <TVirtualPad.h>

PhotoPeakDrawController::PhotoPeakDrawController(TVirtualPad* pad, TH1* histogram)
  : fPad(pad), fHistogram(histogram) {
}

void PhotoPeakDrawController::CleanFitObjects() {
  if(!fPad || !fHistogram || !fPad->GetListOfPrimitives())
    return;
  const TString prefix = TString::Format("PhotoPeak_%p_",
                                         static_cast<void*>(fHistogram));
  std::vector<TObject*> remove;
  TIter next(fPad->GetListOfPrimitives());
  while(TObject* object = next()) {
    if(TString(object->GetName()).BeginsWith(prefix))
      remove.push_back(object);
  }
  for(TObject* object : remove) {
    fPad->GetListOfPrimitives()->Remove(object);
    delete object;
  }
}

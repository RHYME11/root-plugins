#include "PhotoPeakMarkerController.h"

#include <vector>

#include <TH1.h>
#include <TLine.h>
#include <TList.h>
#include <TMarker.h>
#include <TVirtualPad.h>

#include <PhotoPeakFit/PhotoPeakSessionState.h>

namespace {
constexpr UInt_t kPhotoPeakMarkerObject = 0x50500001;
}

PhotoPeakMarkerController::PhotoPeakMarkerController(TVirtualPad* pad,
                                                     TH1* histogram)
  : fPad(pad), fHistogram(histogram) {
}

void PhotoPeakMarkerController::Clear() {
  if(!fPad || !fPad->GetListOfPrimitives())
    return;
  std::vector<TObject*> remove;
  TIter next(fPad->GetListOfPrimitives());
  while(TObject* object = next()) {
    if(object->GetUniqueID() == kPhotoPeakMarkerObject)
      remove.push_back(object);
  }
  for(TObject* object : remove) {
    fPad->GetListOfPrimitives()->Remove(object);
    delete object;
  }
}

void PhotoPeakMarkerController::Refresh(const PhotoPeakSessionState& state) {
  if(!fPad || !fHistogram)
    return;
  Clear();
  const double ymin = fPad->GetUymin();
  const double ymax = fPad->GetUymax();
  const double ranges[2] = {state.request.fitLow, state.request.fitHigh};
  for(double range : ranges) {
    auto* line = new TLine(range, ymin, range, ymax);
    line->SetUniqueID(kPhotoPeakMarkerObject);
    line->SetLineColor(kRed);
    line->SetLineWidth(2);
    line->Draw();
  }
  for(const auto& peak : state.request.peaks) {
    auto* marker = new TMarker(peak.centroid.value,
      fHistogram->GetBinContent(fHistogram->FindBin(peak.centroid.value)), 20);
    marker->SetUniqueID(kPhotoPeakMarkerObject);
    marker->SetMarkerColor(kRed);
    marker->SetMarkerSize(1.2);
    marker->Draw();
  }
  fPad->Modified();
  fPad->Update();
}

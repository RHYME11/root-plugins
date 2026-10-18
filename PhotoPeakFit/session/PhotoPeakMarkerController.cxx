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

class FitRangeLine : public TLine {
public:
  // ============== FitRangeLine ==============
  // Purpose: Identify marker ownership. Inputs: Target and bounds. Outputs: Layer-owned line.
  FitRangeLine(TH1* target, double x, double low, double high)
    : TLine(x, low, x, high), histogram(target) {}
  TH1* histogram;
};

class FitPeakMarker : public TMarker {
public:
  // ============== FitPeakMarker ==============
  // Purpose: Identify marker ownership. Inputs: Target and coordinates. Outputs: Layer-owned marker.
  FitPeakMarker(TH1* target, double x, double y)
    : TMarker(x, y, 20), histogram(target) {}
  TH1* histogram;
};
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
    auto* line = dynamic_cast<FitRangeLine*>(object);
    auto* marker = dynamic_cast<FitPeakMarker*>(object);
    if((line && line->histogram == fHistogram) || (marker && marker->histogram == fHistogram))
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
  if(!state.rangeVisible)
    return;
  const double ymin = fPad->PadtoY(fPad->GetUymin());
  const double ymax = fPad->PadtoY(fPad->GetUymax());
  const double ranges[2] = {state.request.fitLow, state.request.fitHigh};
  for(double range : ranges) {
    auto* line = new FitRangeLine(fHistogram, range, ymin, ymax);
    line->SetUniqueID(kPhotoPeakMarkerObject);
    line->SetLineColor(kRed);
    line->SetLineWidth(2);
    line->Draw();
  }
  for(const auto& peak : state.request.peaks) {
    auto* marker = new FitPeakMarker(fHistogram, peak.centroid.value,
      fHistogram->GetBinContent(fHistogram->FindBin(peak.centroid.value)));
    marker->SetUniqueID(kPhotoPeakMarkerObject);
    marker->SetMarkerColor(peak.id == state.selectedPeak ? kBlue + 1 : kRed);
    marker->SetMarkerSize(peak.id == state.selectedPeak ? 1.5 : 1.2);
    marker->Draw();
  }
  fPad->Modified();
  fPad->Update();
}

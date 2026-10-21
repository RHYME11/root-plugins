#include "PhotoPeakMarkerController.h"

#include <array>
#include <map>
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
  FitRangeLine(TH1* target, int bound, double x, double low, double high)
    : TLine(x, low, x, high), histogram(target), boundary(bound) {}
  TH1* histogram;
  int boundary;
};

class FitPeakMarker : public TMarker {
public:
  // ============== FitPeakMarker ==============
  // Purpose: Identify marker ownership. Inputs: Target and coordinates. Outputs: Layer-owned marker.
  FitPeakMarker(TH1* target, std::uint64_t peak, double x, double y)
    : TMarker(x, y, 20), histogram(target), id(peak) {}
  TH1* histogram;
  std::uint64_t id;
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

// ============== PhotoPeakMarkerController::Refresh ==============
// Purpose: Update overlays in place while ROOT retains the native drag selection.
// Inputs: Committed session state. Outputs: Synchronized range lines and peak markers.
void PhotoPeakMarkerController::Refresh(const PhotoPeakSessionState& state) {
  if(!fPad || !fHistogram)
    return;
  if(!state.rangeVisible) {
    Clear();
    return;
  }
  std::array<FitRangeLine*, 2> lines{};
  std::map<std::uint64_t, FitPeakMarker*> markers;
  TIter next(fPad->GetListOfPrimitives());
  while(auto* object = next()) {
    if(auto* line = dynamic_cast<FitRangeLine*>(object)) {
      if(line->histogram == fHistogram) lines[line->boundary] = line;
    } else if(auto* marker = dynamic_cast<FitPeakMarker*>(object)) {
      if(marker->histogram == fHistogram) markers[marker->id] = marker;
    }
  }
  const double ymin = fPad->PadtoY(fPad->GetUymin());
  const double ymax = fPad->PadtoY(fPad->GetUymax());
  const double ranges[2] = {state.request.fitLow, state.request.fitHigh};
  for(int bound = 0; bound < 2; ++bound) {
    auto* line = lines[bound];
    if(!line) {
      line = new FitRangeLine(fHistogram, bound, ranges[bound], ymin, ymax);
      line->SetUniqueID(kPhotoPeakMarkerObject);
      line->SetLineColor(kRed);
      line->SetLineWidth(2);
      line->Draw();
    }
    line->SetX1(ranges[bound]);
    line->SetX2(ranges[bound]);
    line->SetY1(ymin);
    line->SetY2(ymax);
  }
  for(const auto& peak : state.request.peaks) {
    auto found = markers.find(peak.id);
    auto* marker = found == markers.end() ? nullptr : found->second;
    const double height = fHistogram->GetBinContent(fHistogram->FindBin(peak.centroid.value));
    if(!marker) {
      marker = new FitPeakMarker(fHistogram, peak.id, peak.centroid.value, height);
      marker->SetUniqueID(kPhotoPeakMarkerObject);
      marker->Draw();
    } else {
      markers.erase(found);
    }
    marker->SetX(peak.centroid.value);
    marker->SetY(height);
    marker->SetMarkerColor(peak.id == state.selectedPeak ? kBlue + 1 : kRed);
    marker->SetMarkerSize(peak.id == state.selectedPeak ? 1.5 : 1.2);
  }
  for(const auto& entry : markers) {
    fPad->GetListOfPrimitives()->Remove(entry.second);
    delete entry.second;
  }
  fPad->Modified();
  fPad->Update();
}

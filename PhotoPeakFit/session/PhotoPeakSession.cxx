#include <PhotoPeakFit/PhotoPeakSession.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <Buttons.h>
#include <KeySymbols.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TLine.h>
#include <TList.h>
#include <TMarker.h>
#include <TROOT.h>
#include <TSpectrum.h>
#include <TVirtualPad.h>

#include <PhotoPeakFit/PhotoPeakControlWindow.h>
#include <PhotoPeakFit/PhotoPeakFitter.h>

#include "PhotoPeakDrawController.h"
#include "PhotoPeakMarkerController.h"

namespace {

constexpr UInt_t kPhotoPeakMarkerObject = 0x50500001;
constexpr UInt_t kPhotoPeakBackgroundObject = 0x50500002;

// ============== RemoveSessionObjects ==============
// Purpose: Remove pad primitives owned by one PhotoPeak session category.
// Inputs: Pad and object-name prefix.
// Outputs: Deleted matching primitives.
void RemoveSessionObjects(TVirtualPad* pad, const char* prefix) {
  if(!pad || !pad->GetListOfPrimitives())
    return;
  std::vector<TObject*> remove;
  TIter next(pad->GetListOfPrimitives());
  while(TObject* object = next()) {
    const std::string requested = prefix ? prefix : "";
    const bool special = requested == "PhotoPeakSession_" ?
      object->GetUniqueID() == kPhotoPeakMarkerObject :
      requested == "PhotoPeakSession_background" ?
        object->GetUniqueID() == kPhotoPeakBackgroundObject : false;
    if(special || std::string(object->GetName()).find(requested) == 0)
      remove.push_back(object);
  }
  for(TObject* object : remove) {
    pad->GetListOfPrimitives()->Remove(object);
    delete object;
  }
}

} // namespace

// ============== PhotoPeakSession::PhotoPeakSession ==============
// Purpose: Create one exclusive PhotoPeak pad session.
// Inputs: Owner canvas, event pad, and target histogram.
// Outputs: Initialized session and non-modal control window.
PhotoPeakSession::PhotoPeakSession(TCanvas* canvas, TVirtualPad* pad, TH1* histogram)
  : fCanvas(canvas),
    fPad(pad),
    fHistogram(histogram),
    fRangeClicks(0),
    fDraggingRange(-1),
    fDraggingPeak(-1),
    fPeakMoved(false),
    fPeakWasExisting(false) {
  fMarkers = std::make_unique<PhotoPeakMarkerController>(fPad, fHistogram);
  fDrawing = std::make_unique<PhotoPeakDrawController>(fPad, fHistogram);
  if(fHistogram) {
    fOriginalHistogram.reset(dynamic_cast<TH1*>(
      fHistogram->Clone((std::string(fHistogram->GetName()) +
                         "_photopeak_original").c_str())));
    if(fOriginalHistogram)
      fOriginalHistogram->SetDirectory(nullptr);
    const int first = fHistogram->GetXaxis()->GetFirst();
    const int last = fHistogram->GetXaxis()->GetLast();
    fState.request.fitLow = fHistogram->GetXaxis()->GetBinLowEdge(first);
    fState.request.fitHigh = fHistogram->GetXaxis()->GetBinUpEdge(last);
    fState.request.config.global[kPhotoPeakR] = {
      10.0, PhotoPeakParameterMode::Limited, 0.0, 100.0};
    fState.request.config.global[kPhotoPeakBeta] = {
      0.0, PhotoPeakParameterMode::Limited, 1.0e-6,
      std::max(fState.request.fitHigh - fState.request.fitLow, 1.0)};
    fState.request.config.global[kPhotoPeakStep] = {
      0.25, PhotoPeakParameterMode::Limited, 0.0, 100.0};
  }
  if(!gROOT->IsBatch()) {
    fWindow = std::make_unique<PhotoPeakControlWindow>(this);
    fWindow->Show();
  }
}

PhotoPeakSession::~PhotoPeakSession() {
  Close();
}

// ============== PhotoPeakSession::HandleEvent ==============
// Purpose: Consume every input event for the session-owned pad.
// Inputs: Normalized ROOT-independent event payload.
// Outputs: Always true while the session is active.
bool PhotoPeakSession::HandleEvent(const PhotoPeakInputEvent& event) {
  if(fState.closed || !fHistogram || !fPad)
    return true;
  if(event.type == kButton1Shift) {
    const double tolerance = std::fabs(fPad->AbsPixeltoX(event.px + 7) -
                                       fPad->AbsPixeltoX(event.px));
    const auto found = std::find_if(fState.request.peaks.begin(),
      fState.request.peaks.end(), [&](const PhotoPeakSeed& peak) {
        return std::fabs(peak.centroid.value - event.x) <= tolerance;
      });
    fPeakMoved = false;
    fPeakWasExisting = found != fState.request.peaks.end();
    if(fPeakWasExisting) {
      fDraggingPeak = static_cast<int>(
        std::distance(fState.request.peaks.begin(), found));
    } else {
      AddPeak(event.x);
      const auto added = std::find_if(fState.request.peaks.begin(),
        fState.request.peaks.end(), [&](const PhotoPeakSeed& peak) {
          return peak.centroid.value == event.x;
        });
      fDraggingPeak = added == fState.request.peaks.end() ? -1 :
        static_cast<int>(std::distance(fState.request.peaks.begin(), added));
    }
    RefreshMarkers();
    RefreshGui();
    return true;
  }
  if(event.type == kButton1Down) {
    const double tolerance = std::fabs(fPad->AbsPixeltoX(event.px + 7) -
                                       fPad->AbsPixeltoX(event.px));
    if(std::fabs(event.x - fState.request.fitLow) <= tolerance) {
      fDraggingRange = 0;
      return true;
    }
    if(std::fabs(event.x - fState.request.fitHigh) <= tolerance) {
      fDraggingRange = 1;
      return true;
    }
    double low = fState.request.fitLow;
    double high = fState.request.fitHigh;
    if(fRangeClicks % 2 == 0)
      low = event.x;
    else
      high = event.x;
    fDraggingRange = fRangeClicks % 2;
    ++fRangeClicks;
    SetRange(low, high);
    RefreshGui();
    return true;
  }
  if(event.type == kButton1Motion && fDraggingRange >= 0) {
    const double low = fDraggingRange == 0 ? event.x : fState.request.fitLow;
    const double high = fDraggingRange == 1 ? event.x : fState.request.fitHigh;
    SetRange(low, high);
    RefreshGui();
    return true;
  }
  if(event.type == kButton1Motion && fDraggingPeak >= 0 &&
     fDraggingPeak < static_cast<int>(fState.request.peaks.size())) {
    fState.request.peaks[fDraggingPeak].centroid.value = event.x;
    fPeakMoved = true;
    RefreshMarkers();
    RefreshGui();
    return true;
  }
  if(event.type == kButton1Up) {
    fDraggingRange = -1;
    if(fDraggingPeak >= 0 &&
       fDraggingPeak < static_cast<int>(fState.request.peaks.size())) {
      if(fPeakWasExisting && !fPeakMoved) {
        fState.request.peaks.erase(fState.request.peaks.begin() + fDraggingPeak);
      } else {
        std::sort(fState.request.peaks.begin(), fState.request.peaks.end(),
          [](const PhotoPeakSeed& left, const PhotoPeakSeed& right) {
            return left.centroid.value < right.centroid.value;
          });
      }
      RefreshMarkers();
      RefreshGui();
    }
    fDraggingPeak = -1;
    fPeakMoved = false;
    fPeakWasExisting = false;
    return true;
  }
  if(event.type == kArrowKeyPress) {
    if(event.code == kKey_Left)
      Pan(-1);
    else if(event.code == kKey_Right)
      Pan(1);
    return true;
  }
  if(event.type == kKeyPress) {
    switch(event.code) {
      case kKey_f: Fit(); break;
      case kKey_n: Clean(); break;
      case kKey_w: Rebin(false); break;
      case kKey_q: Rebin(true); break;
      case kKey_b: ShowBackground(); break;
      case kKey_o:
        fHistogram->GetXaxis()->UnZoom();
        fHistogram->GetYaxis()->UnZoom();
        break;
      default: break;
    }
  }
  return true;
}

// ============== PhotoPeakSession::Close ==============
// Purpose: Close UI and remove interactive markers while retaining fit plots.
// Inputs: None.
// Outputs: Closed idempotent session.
void PhotoPeakSession::Close() {
  if(fState.closed)
    return;
  fState.closed = true;
  const bool canvasAlive = fCanvas && gROOT && gROOT->GetListOfCanvases() &&
    gROOT->GetListOfCanvases()->FindObject(fCanvas);
  if(canvasAlive && fMarkers)
    fMarkers->Clear();
  if(fWindow)
    fWindow->Hide();
  if(canvasAlive && fPad) {
    fPad->Modified();
    fPad->Update();
  }
  if(!canvasAlive) {
    fCanvas = nullptr;
    fPad = nullptr;
    fHistogram = nullptr;
  }
}

void PhotoPeakSession::ExitMode() {
  Close();
  if(fExitCallback)
    fExitCallback();
}

void PhotoPeakSession::RaiseWindow() {
  if(fWindow)
    fWindow->Show();
}

// ============== PhotoPeakSession::Fit ==============
// Purpose: Fit current sorted peak seeds and redraw results.
// Inputs: Current session state.
// Outputs: Terminal report and fit primitives.
void PhotoPeakSession::Fit() {
  if(!fHistogram || fState.request.peaks.empty()) {
    std::printf("PhotoPeakFit: select at least one initial peak.\n");
    return;
  }
  fState.request.config.Normalize(fState.request.peaks.size());
  const auto result = PhotoPeakFitter::Fit(fHistogram, fState.request, fPad, true);
  fState.fitAvailable = result.status == 0;
  RefreshGui();
}

void PhotoPeakSession::Clean() {
  if(fMarkers)
    fMarkers->Clear();
  if(fDrawing)
    fDrawing->CleanFitObjects();
  fState.fitAvailable = false;
  if(fPad) {
    fPad->Modified();
    fPad->Update();
  }
}

void PhotoPeakSession::SetRange(double low, double high) {
  if(low > high)
    std::swap(low, high);
  fState.request.fitLow = low;
  fState.request.fitHigh = high;
  RefreshMarkers();
}

void PhotoPeakSession::AddPeak(double centroid) {
  PhotoPeakSeed seed;
  seed.centroid = {centroid, PhotoPeakParameterMode::Free,
                   fState.request.fitLow, fState.request.fitHigh};
  seed.fwhm = {std::sqrt(std::max(9.0 + 0.004 * centroid, 1.0e-12)),
               PhotoPeakParameterMode::Free, 1.0e-6,
               std::fabs(fState.request.fitHigh - fState.request.fitLow)};
  const double background = 0.5 *
    (fHistogram->GetBinContent(fHistogram->FindBin(fState.request.fitLow)) +
     fHistogram->GetBinContent(fHistogram->FindBin(fState.request.fitHigh)));
  seed.height = {std::max(fHistogram->GetBinContent(fHistogram->FindBin(centroid)) -
                          background, 1.0), PhotoPeakParameterMode::Free,
                 0.0, 10.0 * fHistogram->GetMaximum()};
  if(fState.request.peaks.empty() &&
     fState.request.config.global[kPhotoPeakBeta].value == 0.0) {
    fState.request.config.global[kPhotoPeakBeta].value = 0.5 * seed.fwhm.value;
  }
  fState.request.peaks.push_back(seed);
  std::sort(fState.request.peaks.begin(), fState.request.peaks.end(),
    [](const PhotoPeakSeed& left, const PhotoPeakSeed& right) {
      return left.centroid.value < right.centroid.value;
    });
}

void PhotoPeakSession::RemovePeak(std::size_t index) {
  if(index < fState.request.peaks.size())
    fState.request.peaks.erase(fState.request.peaks.begin() + index);
  RefreshMarkers();
  RefreshGui();
}

void PhotoPeakSession::ReplaceRequest(const PhotoPeakFitRequest& request) {
  fState.request = request;
  std::sort(fState.request.peaks.begin(), fState.request.peaks.end(),
    [](const PhotoPeakSeed& left, const PhotoPeakSeed& right) {
      return left.centroid.value < right.centroid.value;
    });
  fState.request.config.Normalize(fState.request.peaks.size());
  RefreshMarkers();
  RefreshGui();
}

void PhotoPeakSession::SetExitCallback(std::function<void()> callback) {
  fExitCallback = std::move(callback);
}

// ============== PhotoPeakSession::RefreshMarkers ==============
// Purpose: Recreate range and centroid markers from state.
// Inputs: Current request.
// Outputs: Synchronized pad primitives.
void PhotoPeakSession::RefreshMarkers() {
  if(fMarkers)
    fMarkers->Refresh(fState);
}

void PhotoPeakSession::RefreshGui() {
  if(fWindow)
    fWindow->Refresh();
}

void PhotoPeakSession::Rebin(bool undo) {
  if(!fHistogram || !fOriginalHistogram)
    return;
  if(undo) {
    if(fState.rebinFactor <= 1)
      return;
    fState.rebinFactor /= 2;
    fOriginalHistogram->Copy(*fHistogram);
    if(fState.rebinFactor > 1)
      fHistogram->Rebin(fState.rebinFactor);
  } else {
    fHistogram->Rebin(2);
    fState.rebinFactor *= 2;
  }
}

void PhotoPeakSession::Pan(int direction) {
  auto* axis = fHistogram ? fHistogram->GetXaxis() : nullptr;
  if(!axis)
    return;
  const double low = axis->GetBinLowEdge(axis->GetFirst());
  const double high = axis->GetBinUpEdge(axis->GetLast());
  const double shift = direction * 0.1 * (high - low);
  axis->SetRangeUser(low + shift, high + shift);
}

void PhotoPeakSession::ShowBackground() {
  if(!fHistogram || !fPad)
    return;
  RemoveSessionObjects(fPad, "PhotoPeakSession_background");
  TSpectrum spectrum;
  TH1* background = spectrum.Background(fHistogram, 20,
    fState.request.config.backgroundOptions.c_str());
  if(!background)
    return;
  background->SetName("PhotoPeakSession_background");
  background->SetUniqueID(kPhotoPeakBackgroundObject);
  background->SetDirectory(nullptr);
  background->SetLineColor(kBlue + 1);
  background->SetLineStyle(2);
  background->Draw("hist same");
}

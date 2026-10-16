#include <PhotoPeakFit/PhotoPeakSession.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include <Buttons.h>
#include <KeySymbols.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TList.h>
#include <TROOT.h>
#include <TVirtualFitter.h>
#include <TVirtualPad.h>

#include <PhotoPeakFit/PhotoPeakControlWindow.h>
#include <PhotoPeakFit/PhotoPeakFitter.h>

#include "PhotoPeakDrawController.h"
#include "PhotoPeakMarkerController.h"
#include "PhotoPeakTarget.h"
#include "PhotoPeakBackground.h"

namespace {

// ============== ReleaseSessionFitter ==============
// Purpose: Release ROOT's retained fitter when it belongs to this session.
// Inputs: Session histogram.
// Outputs: Cleared global fitter without affecting unrelated ROOT fits.
void ReleaseSessionFitter(TH1* histogram) {
  if(!histogram)
    return;
  TVirtualFitter* fitter = TVirtualFitter::GetFitter();
  if(!fitter || fitter->GetObjectFit() != histogram)
    return;
  TVirtualFitter::SetFitter(nullptr);
  delete fitter;
}

} // namespace

// ============== PhotoPeakSession::PhotoPeakSession ==============
// Purpose: Create one PhotoPeak application-overlay session for a pad.
// Inputs: Owner canvas, event pad, and target histogram.
// Outputs: Initialized session and non-modal control window.
PhotoPeakSession::PhotoPeakSession(TCanvas* canvas, TVirtualPad* pad, TH1* histogram)
  : fCanvas(canvas),
    fPad(pad),
    fHistogram(histogram) {
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
    fState.request.config.backgroundLow = fState.request.fitLow;
    fState.request.config.backgroundHigh = fState.request.fitHigh;
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
  Abandon();
}

// ============== PhotoPeakSession::HandleEvent ==============
// Purpose: Apply fit tools after ROOT handles native input.
// Inputs: Pad-local coordinates and key/button identity. Outputs: Updated session.
bool PhotoPeakSession::HandleEvent(const PhotoPeakInputEvent& event) {
  if(!ValidateTarget() || fState.fitting)
    return true;
  if(event.type == kKeyPress) {
    switch(event.code) {
      case kKey_f:
      case kKey_Return:
      case kKey_Enter: Fit(); break;
      case kKey_n: Clean(); break;
      case kKey_w: Rebin(false); break;
      case kKey_q: Rebin(true); break;
      case kKey_b: ShowBackground(); break;
      case kKey_o: Unzoom(); break;
      case kKey_Delete: RemoveSelectedPeak(); break;
      case kKey_Escape: SetInteraction(PhotoPeakInteraction::Browse); break;
      default: break;
    }
    return true;
  }
  if(event.type == kArrowKeyPress) {
    if(event.code == kKey_Left)
      Pan(-1);
    else if(event.code == kKey_Right)
      Pan(1);
    return true;
  }
  if(event.type == kButton1Up) {
    const bool dragging = fDraggingRange >= 0 || fDraggingPeak != 0;
    CancelDrag();
    if(dragging)
      RefreshMarkers();
    return true;
  }
  if(fState.interaction == PhotoPeakInteraction::Browse)
    return true;
  if(event.type == kButton1Down || event.type == kButton1Shift) {
    if(!event.inPlot || !CommitPending())
      return true;
    CancelDrag();
    fDragStart = event.px;
    const double tolerance = std::fabs(fPad->PadtoX(fPad->AbsPixeltoX(event.px + 7)) -
      fPad->PadtoX(fPad->AbsPixeltoX(event.px)));
    const bool rangeTool = event.type == kButton1Shift;
    if(fState.rangeVisible && !rangeTool) {
      if(std::fabs(event.x - fState.request.fitLow) <= tolerance)
        fDraggingRange = 0;
      else if(std::fabs(event.x - fState.request.fitHigh) <= tolerance)
        fDraggingRange = 1;
      if(fDraggingRange >= 0)
        return true;
    }
    if(rangeTool) {
      fDraggingRange = fRangeClicks++ % 2;
      SetRange(fDraggingRange == 0 ? event.x : fState.request.fitLow,
        fDraggingRange == 1 ? event.x : fState.request.fitHigh);
    } else {
      const auto found = std::min_element(fState.request.peaks.begin(),
        fState.request.peaks.end(), [&](const PhotoPeakSeed& a, const PhotoPeakSeed& b) {
          return std::fabs(a.centroid.value - event.x) < std::fabs(b.centroid.value - event.x);
        });
      if(found != fState.request.peaks.end() &&
         std::fabs(found->centroid.value - event.x) <= tolerance)
        SelectPeak(found->id);
      else
        AddPeak(event.x);
      fDraggingPeak = fState.selectedPeak;
    }
    return true;
  }
  if((event.type == kButton1Motion || event.type == kButton1ShiftMotion) &&
     event.inPlot && std::abs(event.px - fDragStart) >= 3) {
    if(fDraggingRange >= 0) {
      double low = fDraggingRange == 0 ? event.x : fState.request.fitLow;
      double high = fDraggingRange == 1 ? event.x : fState.request.fitHigh;
      if(low > high) {
        std::swap(low, high);
        fDraggingRange = 1 - fDraggingRange;
      }
      SetRange(low, high);
    } else if(fDraggingPeak) {
      auto request = fState.request;
      for(auto& peak : request.peaks) {
        if(peak.id == fDraggingPeak) {
          peak.centroid.value = event.x;
          if(peak.centroid.mode == PhotoPeakParameterMode::Limited)
            peak.centroid.value = std::clamp(event.x, peak.centroid.lower, peak.centroid.upper);
        }
      }
      ReplaceRequest(request);
    }
  }
  return true;
}

// ============== PhotoPeakSession::ValidateTarget ==============
// Purpose: Reject removed pads or histograms before GUI and event commands.
// Inputs: Borrowed canvas, pad, and target identities.
// Outputs: True for a live session; otherwise safely released resources.
bool PhotoPeakSession::ValidateTarget() {
  if(fState.closed)
    return false;
  if(IsPhotoPeakPadAlive(fCanvas, fPad) &&
     IsPhotoPeakTargetDrawn(fPad, fHistogram))
    return true;
  Abandon();
  return false;
}

// ============== PhotoPeakSession::DetachTarget ==============
// Purpose: Forget a pad or canvas that is already being destroyed.
// Inputs: None.
// Outputs: Closed session without accessing the removed drawing hierarchy.
void PhotoPeakSession::DetachTarget() {
  ReleaseSessionFitter(fHistogram);
  fCanvas = nullptr;
  fPad = nullptr;
  fHistogram = nullptr;
  Abandon();
}

// ============== PhotoPeakSession::Abandon ==============
// Purpose: Release session resources without dereferencing a removed histogram.
// Inputs: Borrowed identities; target membership is not required.
// Outputs: Empty closed session and safely scheduled control-window deletion.
void PhotoPeakSession::Abandon() {
  if(fState.closed)
    return;
  fState.closed = true;
  ReleaseSessionFitter(fHistogram);
  if(IsPhotoPeakPadAlive(fCanvas, fPad)) {
    if(fMarkers)
      fMarkers->Clear();
    if(fDrawing)
      fDrawing->CleanFitObjects();
    RemovePhotoPeakBackground(fPad, fHistogram);
    fPad->Modified();
  }
  if(fWindow) {
    auto* window = fWindow.release();
    window->DetachSession();
    window->DeleteWindow();
  }
  fOriginalHistogram.reset();
  fMarkers.reset();
  fDrawing.reset();
  fState = PhotoPeakSessionState{};
  fState.closed = true;
  fCanvas = nullptr;
  fPad = nullptr;
  fHistogram = nullptr;
}

// ============== PhotoPeakSession::ExitMode ==============
// Purpose: Discard a complete interactive session.
// Inputs: None.
// Outputs: Closed session with all PhotoPeak resources released.
void PhotoPeakSession::ExitMode() {
  Abandon();
}

// ============== PhotoPeakSession::RaiseWindow ==============
// Purpose: Raise the existing panel without replacing drafts.
// Inputs: None. Outputs: Visible control window for a valid target.
void PhotoPeakSession::RaiseWindow() {
  if(!ValidateTarget())
    return;
  if(fWindow)
    fWindow->Show();
}

// ============== PhotoPeakSession::CommitPending ==============
// Purpose: Share draft submission between GUI and canvas actions.
// Inputs: Current window. Outputs: True when all pending edits are valid.
bool PhotoPeakSession::CommitPending() {
  return !fState.closed && !fState.fitting && (!fWindow || fWindow->CommitEdits());
}

// ============== PhotoPeakSession::SetStatus ==============
// Purpose: Report an operation outcome in the control window.
// Inputs: English message. Outputs: Updated status.
void PhotoPeakSession::SetStatus(const std::string& message) {
  fState.status = message;
  RefreshGui();
}

// ============== PhotoPeakSession::ValidateRequest ==============
// Purpose: Reject invalid inputs before fitting or accepting edits.
// Inputs: Requested parameters. Outputs: Validity and an actionable status.
bool PhotoPeakSession::ValidateRequest(const PhotoPeakFitRequest& request) {
  auto fail = [&](const std::string& message) {
    SetStatus(message);
    return false;
  };
  const double minimum = fHistogram->GetXaxis()->GetXmin();
  const double maximum = fHistogram->GetXaxis()->GetXmax();
  if(!std::isfinite(request.fitLow) || !std::isfinite(request.fitHigh) ||
     request.fitLow >= request.fitHigh || request.fitLow < minimum || request.fitHigh > maximum)
    return fail("Fit range: require histogram minimum <= Low < High <= maximum.");
  auto validControl = [&](const PhotoPeakParameterControl& control, const std::string& label) {
    if(!std::isfinite(control.value) || !std::isfinite(control.lower) || !std::isfinite(control.upper))
      return fail(label + ": enter finite values.");
    if(control.mode == PhotoPeakParameterMode::Limited &&
       (control.lower >= control.upper || control.value < control.lower || control.value > control.upper))
      return fail(label + ": require Lower < Upper and a value within the limits.");
    return true;
  };
  for(const auto& peak : request.peaks) {
    const std::string label = "Peak " + std::to_string(peak.id);
    if(!validControl(peak.centroid, label + " centroid") ||
       !validControl(peak.height, label + " height") ||
       !validControl(peak.fwhm, label + " FWHM"))
      return false;
    if(peak.centroid.value < request.fitLow || peak.centroid.value > request.fitHigh)
      return fail(label + ": centroid must be inside the fit range.");
    if(peak.fwhm.value <= 0.0 || peak.height.value < 0.0)
      return fail(label + ": require FWHM > 0 and height >= 0.");
  }
  auto effective = request.config;
  effective.Normalize(request.peaks.size());
  const int parameters[] = {kPhotoPeakR, kPhotoPeakBeta, kPhotoPeakStep, kPhotoPeakA, kPhotoPeakB, kPhotoPeakC};
  const char* names[] = {"R", "BETA", "STEP", "A", "B", "C"};
  for(int index = 0; index < 6; ++index) {
    if(parameters[index] == kPhotoPeakBeta && request.peaks.empty() &&
       effective.global[kPhotoPeakBeta].value == 0.0)
      continue;
    if(!validControl(effective.global[parameters[index]], names[index]))
      return false;
  }
  if(!validControl(effective.widthScale, "WSCALE") || effective.widthScale.value <= 0.0)
    return fail("WSCALE: require a positive value within valid limits.");
  if(request.config.backgroundIterations < 1)
    return fail("Background iterations: enter a positive integer.");
  if(request.config.background == PhotoPeakBackgroundMode::Local &&
     (!std::isfinite(request.config.backgroundLow) || !std::isfinite(request.config.backgroundHigh) ||
      request.config.backgroundLow >= request.config.backgroundHigh ||
      request.config.backgroundLow < minimum || request.config.backgroundHigh > maximum))
    return fail("Local background range: require minimum <= Low < High <= maximum.");
  return true;
}

// ============== PhotoPeakSession::Fit ==============
// Purpose: Fit a validated snapshot and retain results without replacing seeds.
// Inputs: Committed user settings. Outputs: Result, input snapshot and drawings.
void PhotoPeakSession::Fit() {
  if(!ValidateTarget() || !CommitPending() || !ValidateRequest(fState.request))
    return;
  if(fState.request.peaks.empty()) {
    SetStatus("Select at least one initial peak before fitting.");
    return;
  }
  TVirtualPad::TContext context(fPad);
  fState.fitting = true;
  SetStatus("Fitting...");
  const auto snapshot = fState.request;
  const auto revision = fState.revision;
  auto result = PhotoPeakFitter::Fit(fHistogram, snapshot, fPad, true);
  if(!ValidateTarget())
    return;
  fState.fitting = false;
  fState.fittedRequest = snapshot;
  fState.result = std::move(result);
  fState.hasResult = true;
  fState.resultRevision = revision;
  fState.fittedRebinFactor = fState.rebinFactor;
  SetStatus(!fState.result.message.empty() ? fState.result.message :
    fState.result.status == 0 ? "Fit completed. Results match the current input." :
    "Fit failed or did not converge. Inspect the status and results.");
}

// ============== PhotoPeakSession::Changed ==============
// Purpose: Invalidate drawings while keeping inspectable previous results.
// Inputs: Changed request or binning. Outputs: New input revision.
void PhotoPeakSession::Changed() {
  ++fState.revision;
  ReleaseSessionFitter(fHistogram);
  if(fDrawing)
    fDrawing->CleanFitObjects();
  fState.status = fState.hasResult ? "Input changed. Refit required; previous results are out of date." :
    "Input updated. Ready to fit.";
}

// ============== PhotoPeakSession::CancelDrag ==============
// Purpose: End one mouse gesture without deleting a peak.
// Inputs: None. Outputs: Idle drag state.
void PhotoPeakSession::CancelDrag() {
  fDraggingRange = -1;
  fDraggingPeak = 0;
}

// ============== PhotoPeakSession::Clean ==============
// Purpose: Discard selections, drafts and results while retaining fit options.
// Inputs: Current target. Outputs: Clean canvas and empty session input.
void PhotoPeakSession::Clean() {
  if(!ValidateTarget() || fState.fitting)
    return;
  TVirtualPad::TContext context(fPad);
  Changed();
  if(fMarkers)
    fMarkers->Clear();
  RemovePhotoPeakBackground(fPad, fHistogram);
  fState.request.peaks.clear();
  fNextPeakId = 1;
  fState.selectedPeak = 0;
  fState.rangeVisible = false;
  fState.hasResult = false;
  fState.result = PhotoPeakFitResult{};
  fState.fittedRequest = PhotoPeakFitRequest{};
  const auto* axis = fHistogram->GetXaxis();
  fState.request.fitLow = axis->GetBinLowEdge(axis->GetFirst());
  fState.request.fitHigh = axis->GetBinUpEdge(axis->GetLast());
  fRangeClicks = 0;
  CancelDrag();
  fState.status = "Selections and results cleared. Fit settings and binning retained.";
  if(fWindow)
    fWindow->Refresh(true);
  fPad->Modified();
  fPad->Update();
}

// ============== PhotoPeakSession::SetRange ==============
// Purpose: Set mouse-selected bounds without silently deleting excluded peaks.
// Inputs: Bounds in histogram coordinates. Outputs: Range and synchronized markers.
void PhotoPeakSession::SetRange(double low, double high) {
  if(!ValidateTarget() || !CommitPending() || !std::isfinite(low) || !std::isfinite(high))
    return;
  if(low > high)
    std::swap(low, high);
  low = std::max(low, fHistogram->GetXaxis()->GetXmin());
  high = std::min(high, fHistogram->GetXaxis()->GetXmax());
  if(low >= high)
    return;
  const bool changed = low != fState.request.fitLow || high != fState.request.fitHigh;
  if(changed)
    Changed();
  fState.request.fitLow = low;
  fState.request.fitHigh = high;
  fState.rangeVisible = true;
  RefreshMarkers();
  RefreshGui();
  if(changed && fState.request.config.background == PhotoPeakBackgroundMode::Local)
    ShowBackground();
}

// ============== PhotoPeakSession::SortPeaks ==============
// Purpose: Order seeds while retaining their stable identities.
// Inputs: Current seeds. Outputs: Centroid-ordered request.
void PhotoPeakSession::SortPeaks() {
  std::stable_sort(fState.request.peaks.begin(), fState.request.peaks.end(),
    [](const PhotoPeakSeed& a, const PhotoPeakSeed& b) { return a.centroid.value < b.centroid.value; });
}

// ============== PhotoPeakSession::AddPeak ==============
// Purpose: Add and select one uniquely identified peak.
// Inputs: Exact clicked centroid. Outputs: Seed, editor selection and marker.
void PhotoPeakSession::AddPeak(double centroid) {
  if(!ValidateTarget() || !CommitPending())
    return;
  if(!std::isfinite(centroid) || centroid < fState.request.fitLow || centroid > fState.request.fitHigh) {
    SetStatus("New peak: choose a centroid inside the fit range.");
    return;
  }
  PhotoPeakSeed seed;
  seed.id = fNextPeakId++;
  seed.centroid = {centroid, PhotoPeakParameterMode::Free, fState.request.fitLow, fState.request.fitHigh};
  seed.fwhm = {std::sqrt(std::max(9.0 + 0.004 * centroid, 1.0e-12)),
    PhotoPeakParameterMode::Free, 1.0e-6, fState.request.fitHigh - fState.request.fitLow};
  const double background = 0.5 *
    (fHistogram->GetBinContent(fHistogram->FindBin(fState.request.fitLow)) +
     fHistogram->GetBinContent(fHistogram->FindBin(fState.request.fitHigh)));
  seed.height = {std::max(fHistogram->GetBinContent(fHistogram->FindBin(centroid)) - background, 1.0),
    PhotoPeakParameterMode::Free, 0.0, std::max(10.0 * fHistogram->GetMaximum(), 10.0)};
  if(fState.request.peaks.empty() && fState.request.config.global[kPhotoPeakBeta].value == 0.0)
    fState.request.config.global[kPhotoPeakBeta].value = 0.5 * seed.fwhm.value;
  Changed();
  fState.request.peaks.push_back(seed);
  fState.selectedPeak = seed.id;
  fState.rangeVisible = true;
  SortPeaks();
  RefreshMarkers();
  RefreshGui();
}

// ============== PhotoPeakSession::ReplaceRequest ==============
// Purpose: Atomically accept valid user settings without destructive normalization.
// Inputs: Edited request. Outputs: Success and synchronized state, or retained old state.
bool PhotoPeakSession::ReplaceRequest(const PhotoPeakFitRequest& request) {
  if(!ValidateTarget() || fState.fitting)
    return false;
  auto next = request;
  std::set<std::uint64_t> used;
  for(auto& peak : next.peaks) {
    if(!peak.id || used.count(peak.id))
      peak.id = fNextPeakId++;
    used.insert(peak.id);
    fNextPeakId = std::max(fNextPeakId, peak.id + 1);
  }
  if(!ValidateRequest(next))
    return false;
  auto sameControl = [](const PhotoPeakParameterControl& a, const PhotoPeakParameterControl& b) {
    return a.value == b.value && a.mode == b.mode && a.lower == b.lower && a.upper == b.upper;
  };
  const auto& old = fState.request;
  bool same = old.fitLow == next.fitLow && old.fitHigh == next.fitHigh && old.peaks.size() == next.peaks.size();
  for(std::size_t index = 0; same && index < old.peaks.size(); ++index) {
    const auto& a = old.peaks[index];
    const auto& b = next.peaks[index];
    same = a.id == b.id && sameControl(a.centroid, b.centroid) && sameControl(a.height, b.height) && sameControl(a.fwhm, b.fwhm);
  }
  const auto& a = old.config;
  const auto& b = next.config;
  const bool backgroundChanged = a.background != b.background ||
    (b.background != PhotoPeakBackgroundMode::None &&
      (a.backgroundIterations != b.backgroundIterations || a.backgroundOptions != b.backgroundOptions ||
       a.backgroundLow != b.backgroundLow || a.backgroundHigh != b.backgroundHigh ||
       (b.background == PhotoPeakBackgroundMode::Local &&
        (old.fitLow != next.fitLow || old.fitHigh != next.fitHigh))));
  same = same && a.mode == b.mode && a.background == b.background && a.rootOptions == b.rootOptions &&
    a.backgroundIterations == b.backgroundIterations && a.backgroundOptions == b.backgroundOptions &&
    a.backgroundLow == b.backgroundLow && a.backgroundHigh == b.backgroundHigh &&
    a.relativePosition == b.relativePosition && a.relativeFwhm == b.relativeFwhm && sameControl(a.widthScale, b.widthScale);
  for(int index = 0; same && index < kPhotoPeakNPars; ++index)
    same = sameControl(a.global[index], b.global[index]);
  if(!same) {
    Changed();
    fState.rangeVisible = true;
    fState.request = std::move(next);
    SortPeaks();
    if(!used.count(fState.selectedPeak))
      fState.selectedPeak = fState.request.peaks.empty() ? 0 : fState.request.peaks.front().id;
    RefreshMarkers();
  }
  RefreshGui();
  if(!same && backgroundChanged)
    ShowBackground();
  return true;
}

// ============== PhotoPeakSession::SelectPeak ==============
// Purpose: Select a stable peak identity without changing fit inputs.
// Inputs: Peak ID. Outputs: Highlighted marker and matching parameter editor.
void PhotoPeakSession::SelectPeak(std::uint64_t id) {
  if(!ValidateTarget() || !CommitPending())
    return;
  for(const auto& peak : fState.request.peaks) {
    if(peak.id == id) {
      fState.selectedPeak = id;
      RefreshMarkers();
      RefreshGui();
      return;
    }
  }
}

// ============== PhotoPeakSession::RemoveSelectedPeak ==============
// Purpose: Delete only the explicitly selected peak.
// Inputs: Current selection. Outputs: Remaining seeds retain their identities.
void PhotoPeakSession::RemoveSelectedPeak() {
  if(!ValidateTarget() || !CommitPending())
    return;
  auto request = fState.request;
  request.peaks.erase(std::remove_if(request.peaks.begin(), request.peaks.end(),
    [&](const PhotoPeakSeed& peak) { return peak.id == fState.selectedPeak; }), request.peaks.end());
  ReplaceRequest(request);
}

// ============== PhotoPeakSession::SetInteraction ==============
// Purpose: Switch explicit tools and cancel unfinished drags.
// Inputs: Peaks or Browse. Outputs: Mode and English guidance.
void PhotoPeakSession::SetInteraction(PhotoPeakInteraction mode) {
  if(!ValidateTarget() || fState.fitting)
    return;
  CancelDrag();
  fRangeClicks = 0;
  fState.interaction = mode;
  SetStatus(mode == PhotoPeakInteraction::Browse ? "Browse: fit picking is paused. ROOT interaction remains active." :
    "Pick peaks: click to add or select; drag a marker to move; Shift-click to set the range.");
}

// ============== PhotoPeakSession::RefreshMarkers ==============
// Purpose: Draw committed selection state in the owning pad.
// Inputs: Current state. Outputs: Updated markers.
void PhotoPeakSession::RefreshMarkers() {
  TVirtualPad::TContext context(fPad);
  if(fMarkers)
    fMarkers->Refresh(fState);
}

// ============== PhotoPeakSession::RefreshGui ==============
// Purpose: Update the non-modal window without replacing drafts.
// Inputs: Current state. Outputs: Window refresh.
void PhotoPeakSession::RefreshGui() {
  if(fWindow)
    fWindow->Refresh();
}

// ============== PhotoPeakSession::Rebin ==============
// Purpose: Rebin or undo from the session's original histogram copy.
// Inputs: Undo flag. Outputs: Changed binning, refreshed markers and stale results.
void PhotoPeakSession::Rebin(bool undo) {
  if(!ValidateTarget() || !CommitPending() || !fOriginalHistogram ||
     (undo && fState.rebinFactor <= 1) || (!undo && fHistogram->GetNbinsX() < 2))
    return;
  const auto* axis = fHistogram->GetXaxis();
  const double low = axis->GetBinLowEdge(axis->GetFirst());
  const double high = axis->GetBinUpEdge(axis->GetLast());
  const std::string name = fHistogram->GetName();
  Changed();
  if(undo) {
    fState.rebinFactor /= 2;
    fOriginalHistogram->Copy(*fHistogram);
    fHistogram->SetName(name.c_str());
    if(fState.rebinFactor > 1)
      fHistogram->Rebin(fState.rebinFactor);
  } else {
    fHistogram->Rebin(2);
    fState.rebinFactor *= 2;
  }
  fHistogram->GetXaxis()->SetRangeUser(low, high);
  RemovePhotoPeakBackground(fPad, fHistogram);
  RefreshMarkers();
  RefreshGui();
  fPad->Modified();
  fPad->Update();
  if(fState.request.config.background != PhotoPeakBackgroundMode::None)
    ShowBackground();
}

// ============== PhotoPeakSession::Pan ==============
// Purpose: Move the histogram view by ten percent of its span.
// Inputs: Direction. Outputs: View and markers without invalidating fit inputs.
void PhotoPeakSession::Pan(int direction) {
  if(!ValidateTarget() || !CommitPending())
    return;
  auto* axis = fHistogram->GetXaxis();
  const double low = axis->GetBinLowEdge(axis->GetFirst());
  const double high = axis->GetBinUpEdge(axis->GetLast());
  const double shift = direction * 0.1 * (high - low);
  axis->SetRangeUser(low + shift, high + shift);
  fPad->Modified();
  fPad->Update();
  RefreshMarkers();
}

// ============== PhotoPeakSession::Unzoom ==============
// Purpose: Restore native axis extents. Inputs: None. Outputs: Updated view and markers.
void PhotoPeakSession::Unzoom() {
  if(!ValidateTarget() || !CommitPending())
    return;
  fHistogram->GetXaxis()->UnZoom();
  fHistogram->GetYaxis()->UnZoom();
  fPad->Modified();
  fPad->Update();
  RefreshMarkers();
}

// ============== PhotoPeakSession::RefreshView ==============
// Purpose: Refresh overlays after a native axis dialog without changing fit inputs.
// Inputs: Current view. Outputs: Synchronized markers and protected control drafts.
void PhotoPeakSession::RefreshView() {
  if(!ValidateTarget())
    return;
  RefreshMarkers();
  RefreshGui();
}

// ============== PhotoPeakSession::ShowBackground ==============
// Purpose: Show the TSpectrum background selected for fitting.
// Inputs: Committed background settings. Outputs: Visible estimate used by Fit, or disabled background.
void PhotoPeakSession::ShowBackground() {
  if(!ValidateTarget() || !CommitPending() || !ValidateRequest(fState.request))
    return;
  RemovePhotoPeakBackground(fPad, fHistogram);
  const auto& config = fState.request.config;
  if(config.background == PhotoPeakBackgroundMode::None) {
    fPad->Modified();
    fPad->Update();
    SetStatus("TSpectrum background disabled. Fit uses only its A/B/C and step background.");
    return;
  }
  TVirtualPad::TContext context(fPad);
  auto estimate = EstimatePhotoPeakBackground(fHistogram, config,
    fState.request.fitLow, fState.request.fitHigh);
  if(!estimate.error.empty()) {
    fPad->Modified();
    fPad->Update();
    SetStatus(estimate.error);
    return;
  }
  DrawPhotoPeakBackground(fPad, fHistogram, estimate);
  fPad->Modified();
  fPad->Update();
  SetStatus("Background updated (green dashed line). Fit subtracts it; displayed curves include it.");
}

#include <PhotoPeakFit/PhotoPeakRoot.h>
#include <PhotoPeakFit/PhotoPeakSession.h>
#include <PhotoPeakFit/PhotoPeakFitter.h>
#include <PhotoPeakFit/PhotoPeakControlWindow.h>
#include <PhotoPeakFit/detail/PhotoPeakRootController.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include <Buttons.h>
#include <KeySymbols.h>
#include <TApplication.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TF1.h>
#include <TGButton.h>
#include <TGClient.h>
#include <TGComboBox.h>
#include <TGListBox.h>
#include <TGTab.h>
#include <TGText.h>
#include <TGTextEntry.h>
#include <TGTextView.h>
#include <TGScrollBar.h>
#include <TH1D.h>
#include <TH2D.h>
#include <THashList.h>
#include <TKey.h>
#include <TLine.h>
#include <TLegend.h>
#include <TLegendEntry.h>
#include <TList.h>
#include <TMarker.h>
#include <TROOT.h>
#include <TSystem.h>
#include <TString.h>
#include <TVirtualFitter.h>
#include <TVirtualX.h>

namespace {

// ============== Check ==============
// Purpose: Fail a test with an actionable diagnostic.
// Inputs: Condition and message.
// Outputs: Exception on failure.
void Check(bool condition, const std::string& message) {
  if(!condition)
    throw std::runtime_error(message + " [" + PhotoPeakFit::LastError() + "]");
}

// ============== Markers ==============
// Purpose: Count PhotoPeak-owned range and centroid markers.
// Inputs: Live pad.
// Outputs: Marker count.
int Markers(TVirtualPad* pad) {
  int count = 0;
  TIter next(pad->GetListOfPrimitives());
  while(TObject* object = next())
    count += object->GetUniqueID() == 0x50500001;
  return count;
}

// ============== Emit ==============
// Purpose: Exercise the actual TCanvas signal connection and pad routing.
// Inputs: Canvas, target pad and histogram, event, and histogram x coordinate.
// Outputs: One normalized event through the loaded dictionary receiver.
void Emit(TCanvas& canvas, TVirtualPad* pad, TH1* hist, int type, double x) {
  canvas.SetSelectedPad(static_cast<TPad*>(pad));
  canvas.SetSelected(hist);
  const int px = pad->XtoAbsPixel(pad->XtoPad(x));
  const int py = pad->YtoAbsPixel(0.5 * (pad->GetUymin() + pad->GetUymax()));
  canvas.ProcessedEvent(type, px, py, hist);
}

// ============== Seed ==============
// Purpose: Add a centroid through the left-click peak tool.
// Inputs: Canvas, pad, target, and centroid.
// Outputs: Added peak seed and released drag state.
void Seed(TCanvas& canvas, TVirtualPad* pad, TH1* hist, double x) {
  Emit(canvas, pad, hist, kButton1Down, x);
  Emit(canvas, pad, hist, kButton1Up, x);
}

// ============== FillSpectrum ==============
// Purpose: Provide deterministic, identifiable single or multiple peak data.
// Inputs: Histogram and optional second peak.
// Outputs: Populated bins with Poisson errors.
void FillSpectrum(TH1D& hist, bool multiple = false) {
  hist.SetDirectory(nullptr);
  for(int bin = 1; bin <= hist.GetNbinsX(); ++bin) {
    const double x = hist.GetBinCenter(bin);
    double y = 30.0 + 0.01 * x + 1000.0 * std::exp(-0.5 * std::pow((x - 100.0) / 1.5, 2));
    if(multiple)
      y += 700.0 * std::exp(-0.5 * std::pow((x - 110.0) / 1.5, 2));
    hist.SetBinContent(bin, y);
    hist.SetBinError(bin, std::sqrt(y));
  }
}

// ============== CheckLegend ==============
// Purpose: Check that only fitted centroids identify the colored peak curves.
// Inputs: Drawn pad, histogram and fit snapshot. Outputs: Checked labels and curve references.
void CheckLegend(TVirtualPad* pad, TH1* hist, const PhotoPeakFitResult& result) {
  const TString prefix = TString::Format("PhotoPeak_%p_", static_cast<void*>(hist));
  auto* legend = dynamic_cast<TLegend*>(pad->GetListOfPrimitives()->FindObject(prefix + "legend"));
  Check(legend && legend->GetListOfPrimitives()->GetSize() == static_cast<int>(result.peaks.size()),
    "legend contains one entry per fitted peak");
  for(std::size_t index = 0; index < result.peaks.size(); ++index) {
    auto* entry = dynamic_cast<TLegendEntry*>(legend->GetListOfPrimitives()->At(index));
    const TString curve = result.peaks.size() == 1 ? prefix + "total_fit" :
      prefix + TString::Format("component_%zu", index);
    Check(entry && entry->GetObject() == pad->GetListOfPrimitives()->FindObject(curve) &&
      std::string(entry->GetLabel()) == TString::Format("%.3f", result.peaks[index].centroid).Data() &&
      std::string(entry->GetOption()) == "l", "legend uses only the peak curve and fitted centroid");
  }
}

// ============== FitNumerics ==============
// Purpose: Validate single and multi-peak fitting through unchanged algorithms.
// Inputs: None.
// Outputs: Numerically checked centroids and successful fit statuses.
void FitNumerics() {
  TH1D hist("numeric", "numeric", 1000, 0, 200);
  FillSpectrum(hist);
  auto config = PhotoPeakFitConfig::Defaults();
  config.mode = PhotoPeakFitMode::LowStat;
  auto result = PhotoPeakFitter::Fit(&hist, 90, 120, 100, config, nullptr, false);
  Check(result.status == 0 && std::fabs(result.parameters[kPhotoPeakPosition] - 100) < 0.05,
    "single peak centroid");
  FillSpectrum(hist, true);
  TCanvas canvas("numericCanvas", "numericCanvas", 800, 600);
  hist.Draw();
  canvas.Update();
  PhotoPeakSession session(&canvas, &canvas, &hist);
  session.SetRange(90, 120);
  session.AddPeak(100);
  session.AddPeak(110);
  auto request = session.State().request;
  request.config.mode = PhotoPeakFitMode::LowStat;
  result = PhotoPeakFitter::Fit(&hist, request, nullptr, false);
  Check(result.status == 0 && result.peaks.size() == 2, "multiple peak fit status");
  Check(std::fabs(result.peaks[0].centroid - 100) < 0.05 &&
    std::fabs(result.peaks[1].centroid - 110) < 0.05, "multiple peak centroids");
  session.ExitMode();
}

// ============== PeakDrawing ==============
// Purpose: Check component curves share the fitted background without changing fit results.
// Inputs: Deterministic doublet and low/high-stat modes. Outputs: Curve identities and cleanup checks.
void PeakDrawing() {
  TH1D hist("componentHist", "componentHist", 1000, 0, 200);
  FillSpectrum(hist, true);
  TCanvas canvas("componentCanvas", "componentCanvas", 800, 600);
  hist.GetXaxis()->SetRangeUser(85, 125);
  hist.Draw();
  canvas.Update();
  Check(PhotoPeakFit::Start(&hist, &canvas), "component drawing session");
  PhotoPeakFitRequest request;
  request.fitLow = 90;
  request.fitHigh = 120;
  for(double centroid : {100.0, 110.0}) {
    PhotoPeakSeed seed;
    seed.centroid.value = centroid;
    seed.height.value = centroid == 100.0 ? 1000 : 700;
    seed.fwhm.value = 3.5;
    request.peaks.push_back(seed);
  }
  for(auto mode : {PhotoPeakFitMode::LowStat, PhotoPeakFitMode::HighStat}) {
    request.config.mode = mode;
    request.config.relativePosition = mode == PhotoPeakFitMode::HighStat;
    request.config.global[kPhotoPeakR] = {10, PhotoPeakParameterMode::Fixed, 0, 100};
    request.config.global[kPhotoPeakBeta] = {2, PhotoPeakParameterMode::Fixed, 0, 10};
    request.config.global[kPhotoPeakStep] = {4, PhotoPeakParameterMode::Fixed, 0, 100};
    request.config.global[kPhotoPeakC] = {0, PhotoPeakParameterMode::Fixed, 0, 0};
    const auto baseline = PhotoPeakFitter::Fit(&hist, request, nullptr, false);
    const auto drawn = PhotoPeakFitter::Fit(&hist, request, &canvas, true);
    Check(drawn.model == baseline.model && drawn.status == baseline.status &&
      std::fabs(drawn.area - baseline.area) < 1e-8 * std::max(1.0, baseline.area) &&
      std::fabs(drawn.areaError - baseline.areaError) < 1e-8 * std::max(1.0, baseline.areaError),
      "drawing preserves numerical fit and peak-only area");
    const TString prefix = TString::Format("PhotoPeak_%p_", static_cast<void*>(&hist));
    auto* total = dynamic_cast<TF1*>(canvas.GetListOfPrimitives()->FindObject(prefix + "total_fit"));
    auto* background = dynamic_cast<TF1*>(canvas.GetListOfPrimitives()->FindObject(prefix + "background_fit"));
    auto* first = dynamic_cast<TF1*>(canvas.GetListOfPrimitives()->FindObject(prefix + "component_0"));
    auto* second = dynamic_cast<TF1*>(canvas.GetListOfPrimitives()->FindObject(prefix + "component_1"));
    Check(total && background && first && second, "drawn total, background and components exist");
    CheckLegend(&canvas, &hist, drawn);
    for(int parameter = kPhotoPeakA; parameter <= kPhotoPeakStep; ++parameter) {
      Check(drawn.parameters[parameter] == total->GetParameter(parameter) &&
        drawn.errors[parameter] == total->GetParError(parameter),
        "multi-peak result retains fitted globals and uncertainties");
    }
    Check(drawn.relativeFwhm && drawn.widthScale == total->GetParameter(6) &&
      drawn.widthScaleError == total->GetParError(6), "result retains the fitted width scale");
    const std::string globals = drawn.GlobalParameterReport();
    for(const char* name : {"R", "BETA", "STEP", "A", "B", "C", "WSCALE"})
      Check(globals.find(std::string("  ") + name + ": ") != std::string::npos,
        "shared report includes every global parameter");
    Check(background->Eval(100) > 1, "test has a nonzero fitted background");
    for(double x : {90.0, 97.0, 100.0, 103.0, 106.0, 110.0, 115.0, 120.0}) {
      const double reconstructed = first->Eval(x) + second->Eval(x) - background->Eval(x);
      Check(std::fabs(reconstructed - total->Eval(x)) < 1e-8 * std::max(1.0, std::fabs(total->Eval(x))),
        "sum of components minus duplicated common background equals total");
      Check(first->Eval(x) >= background->Eval(x) - 1e-8 &&
        second->Eval(x) >= background->Eval(x) - 1e-8, "components sit above the full fitted background");
    }
  }
  request.peaks.resize(1);
  const auto single = PhotoPeakFitter::Fit(&hist, request, &canvas, true);
  CheckLegend(&canvas, &hist, single);
  Check(!single.relativeFwhm && single.GlobalParameterReport().find("WSCALE") == std::string::npos,
    "single-peak report omits the unused width scale");
  Check(PhotoPeakFit::Stop(&canvas), "component session exits");
  TIter remaining(canvas.GetListOfPrimitives());
  while(auto* object = remaining())
    Check(std::string(object->GetName()).find("PhotoPeak_") != 0, "component drawings cleaned on exit");
}

// ============== RootInteraction ==============
// Purpose: Exercise routing, original controls, and target selection boundaries.
// Inputs: None.
// Outputs: Checked independent pad sessions and complete clean/re-entry behavior.
void RootInteraction() {
  TH1D first("first", "first", 1000, 0, 200);
  TH1D second("second", "second", 1000, 0, 200);
  FillSpectrum(first);
  FillSpectrum(second);
  TCanvas canvas("interaction", "interaction", 1000, 600);
  canvas.Divide(2, 1);
  TVirtualPad* left = canvas.cd(1);
  first.Draw();
  TVirtualPad* right = canvas.cd(2);
  second.Draw();
  canvas.Update();
  const int initialConnections = canvas.NumberOfConnections();
  Check(PhotoPeakFit::Start(&first, left), "start explicit target");
  Check(PhotoPeakFit::Start(&first, left), "repeat start raises existing session");
  Check(PhotoPeakFit::Start(&second, right), "independent second pad");
  Seed(canvas, left, &first, 100);
  Check(Markers(left) == 3 && Markers(right) == 0, "pad-local centroid event");
  Emit(canvas, left, &first, kButton1Shift, 90);
  Emit(canvas, left, &first, kButton1Motion, 92);
  Emit(canvas, left, &first, kButton1Up, 92);
  Emit(canvas, left, &first, kButton1Shift, 120);
  Emit(canvas, left, &first, kButton1Up, 120);
  canvas.SetSelectedPad(static_cast<TPad*>(left));
  canvas.ProcessedEvent(kKeyPress, 'f', 0, &first);
  bool fitted = false;
  TIter objects(left->GetListOfPrimitives());
  while(TObject* object = objects())
    fitted |= std::string(object->GetName()).find("PhotoPeak_") == 0;
  Check(fitted, "fit shortcut draws result");
  const int bins = first.GetNbinsX();
  canvas.ProcessedEvent(kKeyPress, 'w', 0, &first);
  Check(first.GetNbinsX() == bins / 2 && second.GetNbinsX() == bins,
    "rebin affects only the locked target");
  canvas.ProcessedEvent(kKeyPress, 'q', 0, &first);
  Check(first.GetNbinsX() == bins, "undo rebin shortcut");
  canvas.ProcessedEvent(kKeyPress, 'b', 0, &first);
  Check(!left->GetListOfPrimitives()->FindObject("PhotoPeakSession_background"),
    "background shortcut honors None scope");
  canvas.ProcessedEvent(kKeyPress, 'n', 0, &first);
  Check(Markers(left) == 0 &&
    !left->GetListOfPrimitives()->FindObject("PhotoPeakSession_background"), "clean shortcut");
  canvas.ProcessedEvent(kButton1Down, 2, 2, first.GetXaxis());
  Check(Markers(left) == 0, "axis event does not select fit range");
  Check(PhotoPeakFit::Stop(left), "stop left");
  Check(PhotoPeakFit::Start(&first, left) && Markers(left) == 0, "fresh session re-entry");
  PhotoPeakFit::StopAll();
  Check(canvas.NumberOfConnections() == initialConnections,
    "no canvas hooks after stop all");
  if(!gROOT->IsBatch()) {
    left->cd();
    first.GetXaxis()->SetRangeUser(80, 130);
    left->Modified();
    left->Update();
    Check(PhotoPeakFit::Start(&first, left), "native input startup");
    const int px = left->XtoAbsPixel(100);
    const int py = left->YtoAbsPixel(0.5 * (left->GetUymin() + left->GetUymax()));
    canvas.HandleInput(kButton1Down, px, py);
    canvas.HandleInput(kButton1Up, px, py);
    Check(Markers(left) == 3, "native left click adds a peak");
    Event_t event = {};
    event.fType = kGKeyPress;
    event.fWindow = gVirtualX->GetWindowID(canvas.GetCanvasID());
    event.fCode = gVirtualX->KeysymToKeycode(kKey_Right);
    const int firstBin = first.GetXaxis()->GetFirst();
    gClient->HandleEvent(&event);
    Check(first.GetXaxis()->GetFirst() > firstBin, "native right arrow pans view");
    PhotoPeakFit::Clean(left);
    const int peakY = left->YtoAbsPixel(first.GetBinContent(first.FindBin(100)));
    canvas.HandleInput(kButton1Down, px, peakY);
    canvas.HandleInput(kButton1Up, px, peakY);
    Check(Markers(left) == 3, "native left-click adds a peak");
    canvas.HandleInput(kButton1Down, px, peakY);
    canvas.HandleInput(kButton1Up, px, peakY);
    Check(Markers(left) == 3, "native repeated click selects without deleting");
    canvas.ProcessedEvent(kKeyPress, 0, kKey_Delete, &first);
    Check(Markers(left) == 2, "Delete removes only the selected peak");
    event.fCode = gVirtualX->KeysymToKeycode('n');
    gClient->HandleEvent(&event);
    Check(Markers(left) == 0, "native Clean key after marker deletion");
    const int axisY = left->YtoAbsPixel(left->GetUymin()) + 3;
    const int zoomLow = left->XtoAbsPixel(95);
    const int zoomHigh = left->XtoAbsPixel(115);
    canvas.HandleInput(kButton1Down, zoomLow, axisY);
    canvas.HandleInput(kButton1Motion, zoomHigh, axisY);
    canvas.HandleInput(kButton1Up, zoomHigh, axisY);
    Check(Markers(left) == 0, "native axis drag does not set fit markers");
    Check(first.GetXaxis()->GetBinLowEdge(first.GetXaxis()->GetFirst()) > 90,
      "native axis drag still zooms");
    event.fCode = gVirtualX->KeysymToKeycode(kKey_Escape);
    gClient->HandleEvent(&event);
    Seed(canvas, left, &first, 100);
    Check(Markers(left) == 0, "native Escape enters Browse after ROOT cancels its gesture");
    gROOT->SetEscape(false);
    PhotoPeakFit::StopAll();
  }

  canvas.cd(1);
  left->Clear();
  first.Draw();
  second.Draw("same");
  canvas.SetSelected(nullptr);
  Check(!PhotoPeakFit::Start(nullptr, left), "ambiguous target rejected");
  canvas.SetSelected(&second);
  Check(PhotoPeakFit::Start(nullptr, left), "selected target resolves ambiguity");
  Check(!PhotoPeakFit::Start(&first, left), "target switch requires stop");
  PhotoPeakFit::StopAll();
  left->Clear();
  TH2D twoDimensional("twoD", "twoD", 10, 0, 10, 10, 0, 10);
  twoDimensional.SetDirectory(nullptr);
  twoDimensional.Draw();
  Check(!PhotoPeakFit::Start(&twoDimensional, left), "two-dimensional target rejected");
  left->Clear();
  first.Draw();
  right->cd();
  right->Clear();
  first.Draw();
  Check(PhotoPeakFit::Start(&first, left), "shared target first owner");
  Check(!PhotoPeakFit::Start(&first, right), "shared target second owner rejected");
  PhotoPeakFit::StopAll();

  left->cd();
  canvas.Update();
  left->SetLogx();
  left->Modified();
  left->Update();
  Check(PhotoPeakFit::Start(&first, left), "logarithmic pad startup");
  Seed(canvas, left, &first, 100);
  bool correctLogCoordinate = false;
  TIter markers(left->GetListOfPrimitives());
  while(TObject* object = markers()) {
    if(object->GetUniqueID() != 0x50500001)
      continue;
    if(auto* marker = dynamic_cast<TMarker*>(object))
      correctLogCoordinate |= std::fabs(marker->GetX() - 100) < 2;
  }
  Check(correctLogCoordinate, "logarithmic coordinate conversion");
  PhotoPeakFit::StopAll();
}

// ============== Lifecycle ==============
// Purpose: Verify cleanup on object deletion, drawing removal, and canvas closure.
// Inputs: None.
// Outputs: Invalid sessions reject commands and release their drawing artifacts.
void Lifecycle() {
  TCanvas canvas("lifecycle", "lifecycle", 800, 600);
  auto* hist = new TH1D("deleted", "deleted", 1000, 0, 200);
  FillSpectrum(*hist);
  hist->Draw();
  canvas.Update();
  Check(PhotoPeakFit::Start(hist, &canvas), "deletion startup");
  Seed(canvas, &canvas, hist, 100);
  delete hist;
  Check(!PhotoPeakFit::Clean(&canvas), "deleted target closes session");
  Check(Markers(&canvas) == 0, "deleted target markers cleaned");
  TH1D live("live", "live", 1000, 0, 200);
  FillSpectrum(live);
  live.Draw();
  Check(PhotoPeakFit::Start(&live, &canvas), "removed drawing startup");
  canvas.GetListOfPrimitives()->Remove(&live);
  PhotoPeakRootController::Instance().Poll();
  Check(!PhotoPeakFit::Clean(&canvas), "removed drawing closes session");
  canvas.Divide(2, 1);
  auto* pad = canvas.cd(1);
  live.Draw();
  Check(PhotoPeakFit::Start(&live, pad), "subpad deletion startup");
  delete pad;
  PhotoPeakRootController::Instance().Poll();
  Check(!PhotoPeakFit::Clean(pad), "deleted subpad closes session");
  canvas.cd();
  live.Draw();
  Check(PhotoPeakFit::Start(&live, &canvas), "canvas closure startup");
  canvas.Close();
  Check(!PhotoPeakFit::Clean(&canvas), "closed canvas closes session");
  PhotoPeakFit::StopAll();
}

// ============== FindWidget ==============
// Purpose: Locate real controls without adding production test APIs.
// Inputs: Frame tree and widget ID. Outputs: Matching widget or null.
template<class T>
T* FindWidget(TGCompositeFrame* frame, int id) {
  TIter children(frame->GetList());
  while(auto* element = static_cast<TGFrameElement*>(children())) {
    if(auto* widget = dynamic_cast<T*>(element->fFrame)) {
      auto* identity = dynamic_cast<TGWidget*>(widget);
      if(identity && identity->WidgetId() == id)
        return widget;
    }
    if(auto* composite = dynamic_cast<TGCompositeFrame*>(element->fFrame)) {
      if(auto* found = FindWidget<T>(composite, id))
        return found;
    }
  }
  return nullptr;
}

// ============== CheckLayout ==============
// Purpose: Detect clipped controls in the actual ROOT layout.
// Inputs: Visible frame tree. Outputs: All ordinary children fit inside their parent.
void CheckLayout(TGCompositeFrame* frame) {
  TIter children(frame->GetList());
  while(auto* element = static_cast<TGFrameElement*>(children())) {
    auto* child = element->fFrame;
    if(!frame->IsVisible(child))
      continue;
    Check(child->GetX() >= 0 && child->GetY() >= 0 &&
      child->GetX() + child->GetWidth() <= frame->GetWidth() &&
      child->GetY() + child->GetHeight() <= frame->GetHeight(),
      std::string("control fits parent: ") + child->ClassName());
    if(dynamic_cast<TGListBox*>(child) || dynamic_cast<TGTextView*>(child) || dynamic_cast<TGTab*>(child))
      continue;
    if(auto* composite = dynamic_cast<TGCompositeFrame*>(child))
      CheckLayout(composite);
  }
}

// ============== GuiControls ==============
// Purpose: Verify real English controls, draft submission, stable selection and results.
// Inputs: Interactive ROOT application. Outputs: Checked four-page control panel.
void GuiControls() {
  TCanvas canvas("guiControls", "guiControls", 800, 600);
  TH1D hist("guiHist", "guiHist", 1000, 0, 200);
  FillSpectrum(hist);
  hist.Draw();
  canvas.Update();
  PhotoPeakSession session(&canvas, &canvas, &hist);
  PhotoPeakControlWindow* window = nullptr;
  TIter windows(gClient->GetListOfWindows());
  while(TObject* object = windows()) {
    if(auto* control = dynamic_cast<PhotoPeakControlWindow*>(object)) {
      if(control->IsMapped())
        window = control;
    }
  }
  Check(window, "English control window created");
  auto* initialList = FindWidget<TGListBox>(window, 110);
  auto* initialResults = FindWidget<TGTextView>(window, -1);
  Check(initialList && !initialList->GetScrollBar()->IsMapped(),
    "empty peak list has no mapped scrollbar on first show");
  Check(initialResults, "results view exists on first show");
  auto* relativeFwhm = FindWidget<TGCheckButton>(window, 109);
  Check(relativeFwhm && !relativeFwhm->IsEnabled() && relativeFwhm->IsDisabledAndSelected(),
    "stored relative-width preference is visibly checked while inactive");
  TIter resultChildren(initialResults->GetList());
  while(auto* element = static_cast<TGFrameElement*>(resultChildren())) {
    if(auto* scrollbar = dynamic_cast<TGScrollBar*>(element->fFrame))
      Check(!scrollbar->IsMapped(), "empty results view has no mapped scrollbar on first show");
  }
  window->Hide();
  window->Show();
  Check(!initialList->GetScrollBar()->IsMapped(), "raising an empty panel does not remap its scrollbar");
  CheckLayout(window);
  TIter frames(window->GetList());
  while(auto* element = static_cast<TGFrameElement*>(frames())) {
    if(auto* tabs = dynamic_cast<TGTab*>(element->fFrame)) {
      Check(tabs->GetNumberOfTabs() == 4, "four configuration pages");
      for(int index = 0; index < 4; ++index) {
        tabs->SetTab(index, false);
        window->Layout();
        CheckLayout(tabs->GetTabContainer(index));
      }
      tabs->SetTab(0, false);
    }
  }
  session.SetRange(80, 130);
  for(int index = 0; index < 25; ++index)
    session.AddPeak(90 + index);
  auto* list = FindWidget<TGListBox>(window, 110);
  auto* centroid = FindWidget<TGTextEntry>(window, 1000);
  auto* fwhm = FindWidget<TGTextEntry>(window, 1020);
  auto* mode = FindWidget<TGComboBox>(window, 105);
  auto* range = FindWidget<TGTextEntry>(window, 10);
  Check(list && centroid && fwhm && mode && range, "peak list and selected-peak editors exist");
  Check(relativeFwhm->IsEnabled() && relativeFwhm->IsOn() && !fwhm->IsEnabled(),
    "checked relative FWHM explains the disabled individual-width editor");
  relativeFwhm->SetState(kButtonUp, false);
  window->ProcessMessage(MK_MSG(kC_COMMAND, kCM_CHECKBUTTON), 109, 0);
  Check(!session.State().request.config.relativeFwhm && !relativeFwhm->IsOn() && fwhm->IsEnabled(),
    "unchecking relative FWHM enables individual widths");
  window->Refresh();
  Check(!relativeFwhm->IsOn() && fwhm->IsEnabled(), "refresh retains the unchecked width preference");
  relativeFwhm->SetState(kButtonDown, false);
  window->ProcessMessage(MK_MSG(kC_COMMAND, kCM_CHECKBUTTON), 109, 0);
  Check(session.State().request.config.relativeFwhm && relativeFwhm->IsOn() && !fwhm->IsEnabled(),
    "checking relative FWHM restores the shared-width state");
  auto* scrollbar = list->GetScrollBar();
  Check(scrollbar->IsMapped() && scrollbar->GetX() >= 0 && scrollbar->GetY() >= 0 &&
    scrollbar->GetX() + scrollbar->GetWidth() <= list->GetWidth() &&
    scrollbar->GetY() + scrollbar->GetHeight() <= list->GetHeight(),
    "populated peak list has a scrollbar inside its own bounds");
  const auto id = session.State().selectedPeak;
  const Long_t button = MK_MSG(kC_COMMAND, kCM_BUTTON);
  const Long_t edited = MK_MSG(kC_TEXTENTRY, kTE_TEXTCHANGED);
  centroid->SetText("89.5", false);
  window->ProcessMessage(edited, 1000, 0);
  window->Refresh();
  Check(std::string(centroid->GetText()) == "89.5", "refresh preserves pending text");
  Check(window->CommitEdits(), "commit selected peak edit");
  Check(session.State().request.peaks.front().id == id && list->GetSelected() == static_cast<int>(id),
    "sorting retains stable selection across 25 peaks");
  range->SetText("not-a-number", false);
  window->ProcessMessage(edited, 10, 0);
  const auto revision = session.State().revision;
  PhotoPeakInputEvent fitKey;
  fitKey.type = kKeyPress;
  fitKey.code = kKey_f;
  session.HandleEvent(fitKey);
  Check(session.State().revision == revision && !session.State().hasResult &&
    std::string(range->GetText()) == "not-a-number", "canvas Fit rejects and preserves invalid draft");
  range->SetText("80", false);
  window->ProcessMessage(edited, 10, 0);
  Check(window->CommitEdits(), "correct invalid range");
  centroid->SetText("89.7", false);
  window->ProcessMessage(edited, 1000, 0);
  Event_t focus = {};
  focus.fType = kFocusOut;
  centroid->HandleFocusChange(&focus);
  gSystem->ProcessEvents();
  Check(session.State().request.peaks.front().centroid.value == 89.7,
    "losing numeric field focus commits the draft");
  window->ProcessMessage(button, 114, 0);
  Check(session.State().request.peaks.size() == 24, "Delete selected removes exactly one peak");
  mode->Select(2, false);
  window->ProcessMessage(MK_MSG(kC_COMMAND, kCM_COMBOBOX), 105, 2);
  Check(session.State().request.config.mode == PhotoPeakFitMode::LowStat &&
    session.State().request.config.global[kPhotoPeakR].value == 10,
    "LowStat preserves stored global values");
  mode->Select(1, false);
  window->ProcessMessage(MK_MSG(kC_COMMAND, kCM_COMBOBOX), 105, 1);
  Check(session.State().request.config.global[kPhotoPeakR].value == 10, "HighStat restores stored settings");
  fwhm->SetText("-2", false);
  window->ProcessMessage(edited, 1020, 0);
  window->ProcessMessage(button, 101, 0);
  Check(session.State().request.peaks.empty() && Markers(&canvas) == 0,
    "Clean discards invalid drafts and all selections");
  session.AddPeak(100);
  Check(session.State().selectedPeak == 1 && list->GetSelected() == 1,
    "Clean restarts peak numbering in the session and visible list");
  mode->Select(2, false);
  window->ProcessMessage(MK_MSG(kC_COMMAND, kCM_COMBOBOX), 105, 2);
  Check(session.State().request.config.relativeFwhm && relativeFwhm->IsDisabledAndSelected() && fwhm->IsEnabled(),
    "editing a single peak preserves the inactive shared-width preference");
  auto* tabs = FindWidget<TGTab>(window, -1);
  Check(tabs, "result tab control exists");
  tabs->SetTab(3, false);
  centroid->SetText("100.2", false);
  window->ProcessMessage(edited, 1000, 0);
  session.HandleEvent(fitKey);
  Check(session.State().hasResult && session.State().fittedRequest.peaks.front().centroid.value == 100.2,
    "canvas Fit commits the same real editor as Fit button");
  Check(session.State().result.status == 0, "GUI-driven fit converges");
  std::string resultText = initialResults->GetText()->AsString().Data();
  Check(resultText.find(session.State().result.GlobalParameterReport()) != std::string::npos &&
    resultText.find("Centroid: ") != std::string::npos && resultText.find("No fit result") == std::string::npos,
    "visible Results contains the completed snapshot before any further event or interaction");
  const double shortcutCentroid = session.State().result.peaks.front().centroid;
  window->ProcessMessage(button, 100, 0);
  Check(std::fabs(session.State().result.peaks.front().centroid - shortcutCentroid) < 1e-8,
    "button and shortcut fit identical inputs");
  auto request = session.State().request;
  request.peaks.front().centroid = {100.4, PhotoPeakParameterMode::Fixed, 0, 0};
  Check(session.ReplaceRequest(request), "change the next fit to a distinct fixed centroid");
  window->ProcessMessage(button, 100, 0);
  resultText = initialResults->GetText()->AsString().Data();
  Check(resultText.find("Centroid: 100.4 +/- 0") != std::string::npos && tabs->GetCurrent() == 3,
    "refit immediately replaces visible Results while preserving the selected tab");
  window->CloseWindow();
  Check(session.IsClosed(), "window close exits session");
  gSystem->ProcessEvents();
}

// ============== SessionState ==============
// Purpose: Verify stable identities, tool modes, mode preferences and result revisions.
// Inputs: Deterministic multi-peak data. Outputs: Checked state transitions and preview scope.
void SessionState() {
  TCanvas canvas("stateCanvas", "stateCanvas", 800, 600);
  TH1D hist("stateHist", "stateHist", 1000, 0, 200);
  FillSpectrum(hist, true);
  hist.Draw();
  canvas.Update();
  PhotoPeakSession session(&canvas, &canvas, &hist);
  session.SetRange(90, 120);
  session.AddPeak(100);
  const auto first = session.State().selectedPeak;
  session.AddPeak(110);
  const auto second = session.State().selectedPeak;
  Check(session.State().request.config.relativeFwhm, "adding first peak preserves relative-width preference");
  auto request = session.State().request;
  request.config.mode = PhotoPeakFitMode::LowStat;
  Check(session.ReplaceRequest(request), "accept LowStat without normalizing user settings");
  session.Fit();
  Check(session.State().hasResult && session.State().result.status == 0, "session retains successful fit");
  const auto direct = PhotoPeakFitter::Fit(&hist, session.State().fittedRequest, nullptr, false);
  Check(std::fabs(session.State().result.peaks[0].centroid - direct.peaks[0].centroid) < 1e-8,
    "session fit matches unchanged fitter for the same snapshot");
  const auto fittedRevision = session.State().revision;
  session.SelectPeak(first);
  Check(session.State().revision == fittedRevision, "selection does not invalidate results");
  request = session.State().request;
  request.peaks[1].centroid.value = 95;
  Check(session.ReplaceRequest(request), "move second peak across first");
  Check(session.State().request.peaks.front().id == second && session.State().selectedPeak == first &&
    session.State().resultRevision != session.State().revision, "sorting preserves IDs and invalidates results");
  request = session.State().request;
  request.peaks[0].fwhm.value = -1;
  const auto revision = session.State().revision;
  Check(!session.ReplaceRequest(request) && session.State().revision == revision, "invalid FWHM is atomic");
  session.SetInteraction(PhotoPeakInteraction::Browse);
  PhotoPeakInputEvent click;
  click.type = kButton1Down;
  click.x = 105;
  click.px = canvas.XtoAbsPixel(105);
  session.HandleEvent(click);
  Check(session.State().request.peaks.size() == 2, "Browse pauses fit picking");
  session.SetInteraction(PhotoPeakInteraction::Peaks);
  click.x = 100;
  click.px = canvas.XtoAbsPixel(100);
  session.HandleEvent(click);
  click.type = kButton1Up;
  session.HandleEvent(click);
  Check(session.State().request.peaks.size() == 2 && session.State().selectedPeak == first,
    "repeated peak click selects without deleting");
  click.type = kButton1Down;
  click.x = 100;
  click.px = canvas.XtoAbsPixel(100);
  session.HandleEvent(click);
  click.type = kButton1Motion;
  click.x = 94;
  click.px = canvas.XtoAbsPixel(94);
  session.HandleEvent(click);
  click.type = kButton1Up;
  session.HandleEvent(click);
  Check(session.State().request.peaks.front().id == first && session.State().selectedPeak == first,
    "dragging across another peak preserves selected identity");
  click.type = kButton1Down;
  click.x = 90;
  click.px = canvas.XtoAbsPixel(90);
  session.HandleEvent(click);
  click.type = kButton1Motion;
  click.x = 125;
  click.px = canvas.XtoAbsPixel(125);
  session.HandleEvent(click);
  click.x = 130;
  click.px = canvas.XtoAbsPixel(130);
  session.HandleEvent(click);
  click.type = kButton1Up;
  session.HandleEvent(click);
  Check(session.State().request.fitLow == 120 && session.State().request.fitHigh == 130,
    "crossing a range boundary keeps the dragged endpoint under the pointer");
  session.SetRange(90, 120);
  canvas.SetLogy();
  canvas.Modified();
  canvas.Update();
  session.SelectPeak(first);
  TIter primitives(canvas.GetListOfPrimitives());
  bool logarithmicLine = false;
  while(auto* object = primitives()) {
    if(auto* line = dynamic_cast<TLine*>(object))
      logarithmicLine |= line->GetUniqueID() == 0x50500001 &&
        std::fabs(line->GetY2() - canvas.PadtoY(canvas.GetUymax())) < 1e-6;
  }
  Check(logarithmicLine, "range markers use physical y coordinates on log axes");
  request = session.State().request;
  request.config.background = PhotoPeakBackgroundMode::Local;
  request.config.backgroundLow = 90;
  request.config.backgroundHigh = 120;
  request.config.backgroundIterations = 5;
  Check(session.ReplaceRequest(request), "accept local background settings");
  const int firstBin = hist.GetXaxis()->GetFirst();
  const int lastBin = hist.GetXaxis()->GetLast();
  session.ShowBackground();
  auto* background = dynamic_cast<TH1*>(canvas.GetListOfPrimitives()->FindObject("PhotoPeakSession_background"));
  Check(background && background->GetBinContent(hist.FindBin(50)) == 0 &&
    background->GetBinContent(hist.FindBin(100)) > 0, "local preview uses requested range");
  Check(hist.GetXaxis()->GetFirst() == firstBin && hist.GetXaxis()->GetLast() == lastBin,
    "background preview preserves source axis range");
  request.config.background = PhotoPeakBackgroundMode::Global;
  request.config.backgroundIterations = 9;
  Check(session.ReplaceRequest(request), "accept global preview settings");
  session.ShowBackground();
  background = dynamic_cast<TH1*>(canvas.GetListOfPrimitives()->FindObject("PhotoPeakSession_background"));
  Check(background && background->GetBinContent(hist.FindBin(50)) > 0, "global preview covers full spectrum");
  request.config.background = PhotoPeakBackgroundMode::None;
  Check(session.ReplaceRequest(request), "disable background preview");
  session.ShowBackground();
  Check(!canvas.GetListOfPrimitives()->FindObject("PhotoPeakSession_background"), "None clears preview");
  session.Rebin(false);
  Check(session.State().rebinFactor == 2 && session.State().resultRevision != session.State().revision,
    "rebin invalidates previous result");
  session.Rebin(true);
  Check(hist.GetNbinsX() == 1000 && std::string(hist.GetName()) == "stateHist", "undo preserves histogram identity");
  session.Clean();
  Check(!session.State().hasResult && session.State().request.config.mode == PhotoPeakFitMode::LowStat &&
    session.State().request.config.global[kPhotoPeakR].value == 10, "Clean retains stored settings");
  for(int cycle = 0; cycle < 3; ++cycle) {
    session.AddPeak(110);
    session.AddPeak(100);
    Check(session.State().request.peaks.size() == 2 &&
      session.State().request.peaks.front().id == 2 && session.State().request.peaks.back().id == 1,
      "each Clean starts a new selection with IDs one and two, independent of sorting");
    session.Clean();
    Check(session.State().request.peaks.empty() && session.State().selectedPeak == 0 && Markers(&canvas) == 0,
      "repeated Clean removes every prior seed and marker");
  }
  session.ExitMode();
}

// ============== RealSpectra ==============
// Purpose: Validate fitting and independent sessions using the supplied ROOT data.
// Inputs: Read-only fixture path.
// Outputs: Checked real-data centroids and multiple independent TH1D sessions.
void RealSpectra(const char* path) {
  std::unique_ptr<TFile> file(TFile::Open(path, "READ"));
  Check(file && !file->IsZombie(), "open real spectrum fixture");
  TCanvas canvas("real", "real", 1000, 600);
  canvas.Divide(2, 1);
  for(int i = 0; i < 2; ++i) {
    auto* hist = file->Get<TH1D>(i == 0 ? "Array16/energy" : "Array17/energy");
    Check(hist, "fixture contains requested TH1D");
    auto* pad = canvas.cd(i + 1);
    hist->GetXaxis()->SetRangeUser(320, 370);
    hist->Draw();
    canvas.Update();
    Check(PhotoPeakFit::Start(hist, pad), "real spectrum session");
    Seed(canvas, pad, hist, 344);
    auto config = PhotoPeakFitConfig::Defaults();
    config.mode = PhotoPeakFitMode::LowStat;
    auto result = PhotoPeakFitter::Fit(hist, 330, 357, 344, config, pad, true);
    Check(result.status == 0 && result.parameters[kPhotoPeakPosition] > 340 &&
      result.parameters[kPhotoPeakPosition] < 348, "real spectrum peak fit");
  }
  PhotoPeakFit::StopAll();
  Check(Markers(canvas.GetPad(1)) == 0 && Markers(canvas.GetPad(2)) == 0,
    "real spectrum sessions cleanly exit");
  file->Close();
}

} // namespace

// ============== main ==============
// Purpose: Run focused ROOT integration and optional GUI/real-spectrum checks.
// Inputs: Optional --gui or --data PATH.
// Outputs: Test report and process status.
int main(int argc, char** argv) {
  const bool gui = argc > 1 && std::string(argv[1]) == "--gui";
  int appArgc = gui ? 1 : 2;
  char batch[] = "-b";
  char* appArgv[] = {argv[0], batch, nullptr};
  TApplication app("PhotoPeakTests", &appArgc, appArgv);
  try {
    if(gui)
      Check(gClient && !gROOT->IsBatch(), "native GUI unavailable; run from an interactive desktop session");
    if(argc > 2 && std::string(argv[1]) == "--data") {
      RealSpectra(argv[2]);
    } else {
      FitNumerics();
      PeakDrawing();
      RootInteraction();
      Lifecycle();
      SessionState();
      if(gui)
        GuiControls();
    }
    PhotoPeakFit::StopAll();
    std::cout << "PASS PhotoPeakFit ROOT integration" << std::endl;
    return 0;
  } catch(const std::exception& error) {
    PhotoPeakFit::StopAll();
    std::cerr << "FAIL " << error.what() << std::endl;
    return 1;
  }
}

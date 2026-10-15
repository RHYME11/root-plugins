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
#include <TGButton.h>
#include <TGClient.h>
#include <TH1D.h>
#include <TH2D.h>
#include <THashList.h>
#include <TKey.h>
#include <TLine.h>
#include <TList.h>
#include <TMarker.h>
#include <TROOT.h>
#include <TSystem.h>
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
// Purpose: Add a centroid through the original Shift-click interaction.
// Inputs: Canvas, pad, target, and centroid.
// Outputs: Added peak seed and released drag state.
void Seed(TCanvas& canvas, TVirtualPad* pad, TH1* hist, double x) {
  Emit(canvas, pad, hist, kButton1Shift, x);
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
  Emit(canvas, left, &first, kButton1Down, 90);
  Emit(canvas, left, &first, kButton1Motion, 92);
  Emit(canvas, left, &first, kButton1Up, 92);
  Emit(canvas, left, &first, kButton1Down, 120);
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
  Check(left->GetListOfPrimitives()->FindObject("PhotoPeakSession_background"),
    "background shortcut");
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
    Check(Markers(left) == 2, "native plot click reaches session");
    Event_t event = {};
    event.fType = kGKeyPress;
    event.fWindow = gVirtualX->GetWindowID(canvas.GetCanvasID());
    event.fCode = gVirtualX->KeysymToKeycode(kKey_Right);
    const int firstBin = first.GetXaxis()->GetFirst();
    gClient->HandleEvent(&event);
    Check(first.GetXaxis()->GetFirst() > firstBin, "native right arrow pans view");
    PhotoPeakFit::Clean(left);
    const int peakY = left->YtoAbsPixel(first.GetBinContent(first.FindBin(100)));
    canvas.HandleInput(kButton1Shift, px, peakY);
    canvas.HandleInput(kButton1Up, px, peakY);
    Check(Markers(left) == 3, "native Shift-click adds a peak");
    canvas.HandleInput(kButton1Shift, px, peakY);
    canvas.HandleInput(kButton1Up, px, peakY);
    Check(Markers(left) == 2, "native Shift-click removes the peak");
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

// ============== GuiControls ==============
// Purpose: Verify retained pagination, Delete, Clean, and window-close commands.
// Inputs: Interactive ROOT application.
// Outputs: Original three-row pool and synchronized request state.
void GuiControls() {
  TCanvas canvas("guiControls", "guiControls", 800, 600);
  TH1D hist("guiHist", "guiHist", 1000, 0, 200);
  FillSpectrum(hist);
  hist.Draw();
  PhotoPeakSession session(&canvas, &canvas, &hist);
  PhotoPeakControlWindow* window = nullptr;
  TIter windows(gClient->GetListOfWindows());
  while(TObject* object = windows()) {
    if(auto* control = dynamic_cast<PhotoPeakControlWindow*>(object)) {
      if(control->IsMapped())
        window = control;
    }
  }
  Check(window, "control window created");
  for(int i = 0; i < 7; ++i)
    session.AddPeak(95 + i);
  const Long_t button = MK_MSG(kC_COMMAND, kCM_BUTTON);
  window->ProcessMessage(button, 2000, 0);
  Check(session.State().request.peaks.size() == 6, "Delete removes one peak on last page");
  window->ProcessMessage(button, 200, 0);
  window->ProcessMessage(button, 2000, 0);
  Check(session.State().request.peaks.size() == 5, "Previous page and Delete");
  window->ProcessMessage(button, 101, 0);
  Check(session.State().request.peaks.empty() && Markers(&canvas) == 0, "GUI Clean");
  window->CloseWindow();
  Check(session.IsClosed(), "window close exits session");
  gSystem->ProcessEvents();
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
    if(argc > 2 && std::string(argv[1]) == "--data") {
      RealSpectra(argv[2]);
    } else {
      FitNumerics();
      RootInteraction();
      Lifecycle();
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

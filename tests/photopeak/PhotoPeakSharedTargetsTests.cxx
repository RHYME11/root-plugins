#include <PhotoPeakFit/PhotoPeakRoot.h>
#include <PhotoPeakFit/PhotoPeakSession.h>
#include <PhotoPeakFit/PhotoPeakControlWindow.h>
#include <PhotoPeakFit/detail/PhotoPeakRootController.h>
#include <TApplication.h>
#include <TCanvas.h>
#include <TH1D.h>
#include <TList.h>
#include <THashList.h>
#include <TROOT.h>
#include <TString.h>
#include <TGClient.h>
#include <TGComboBox.h>
#include <TGTextEntry.h>
#include <TImage.h>
#include <TPave.h>
#include <Buttons.h>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
// ============== Check ==============
// Purpose: Test helper. Inputs: Target data. Outputs: Checked behavior.
void Check(bool condition, const std::string& message) {
  if(!condition)
    throw std::runtime_error(message + " [" + PhotoPeakFit::LastError() + "]");
}

// ============== Emit ==============
// Purpose: Test helper. Inputs: Target data. Outputs: Checked behavior.
void Emit(TCanvas& canvas, TVirtualPad* pad, TH1* hist, int type, double x) {
  canvas.SetSelectedPad(static_cast<TPad*>(pad));
  canvas.SetSelected(hist);
  const int px = pad->XtoAbsPixel(pad->XtoPad(x));
  const int py = pad->YtoAbsPixel(0.5 * (pad->GetUymin() + pad->GetUymax()));
  canvas.ProcessedEvent(type, px, py, hist);
}

// ============== FillSpectrum ==============
// Purpose: Test helper. Inputs: Target data. Outputs: Checked behavior.
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

// ============== template<class T> ==============
// Purpose: Test helper. Inputs: Target data. Outputs: Checked behavior.
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

// ============== Seed ==============
// Purpose: Dispatch a full click. Inputs: Target and coordinate. Outputs: Controller event.
void Seed(TCanvas& canvas, TVirtualPad* pad, TH1* hist, double x) {
  Emit(canvas, pad, hist, kButton1Down, x);
  Emit(canvas, pad, hist, kButton1Up, x);
}
// ============== SharedTargets ==============
// Purpose: Verify pad/layer selection, independent drafts, overwrite, undo and cleanup.
// Inputs: Native GUI availability. Outputs: Behavioral assertions through real controller events.
void SharedTargets(bool gui) {
  auto& controller = PhotoPeakRootController::Instance();
  PhotoPeakFit::StopAll();
  TCanvas canvas("sharedTargets", "Shared PhotoPeakFit targets", 1000, 700);
  canvas.Divide(2, 1);
  TH1D a("layerA", "A", 1000, 0, 200);
  TH1D b("layerB", "B", 1000, 0, 200);
  TH1D c("layerC", "C", 1000, 0, 80);
  FillSpectrum(a); FillSpectrum(b); FillSpectrum(c);
  auto* left = canvas.cd(1); a.Draw(); b.Draw("same");
  auto* right = canvas.cd(2); c.Draw(); canvas.Update();
  Check(PhotoPeakFit::Start(&a, left), "start shared editor");
  bool borderFound = false;
  TIter objects(left->GetListOfPrimitives());
  while(auto* object = objects()) {
    auto* border = dynamic_cast<TPave*>(object);
    if(border && border->TestBit(TObject::kCannotPick) && border->GetLineColor() == kRed)
      borderFound = border->GetBorderSize() == 1;
  }
  Check(borderFound, "active outline is visible and cannot capture ROOT selection");
  auto* sa = controller.ActiveSession();
  sa->SetRange(90, 120); sa->AddPeak(100);
  PhotoPeakControlWindow* panel = nullptr;
  if(gui) {
    TIter windows(gClient->GetListOfWindows());
    while(auto* object = windows()) {
      auto* candidate = dynamic_cast<PhotoPeakControlWindow*>(object);
      if(candidate && candidate->IsMapped()) panel = candidate;
    }
    Check(panel && FindWidget<TGComboBox>(panel, 800), "shared fit-layer selector exists");
    Check(FindWidget<TGComboBox>(panel, 800)->GetHeight() >= 24, "fit layer selector has usable height");
    auto* low = FindWidget<TGTextEntry>(panel, 10);
    low->SetText("unfinished", false);
    panel->ProcessMessage(MK_MSG(kC_TEXTENTRY, kTE_TEXTCHANGED), 10, 0);
  }
  Check(controller.SelectLayer(2), "select second histogram layer");
  auto* sb = controller.ActiveSession();
  Check(sb != sa && sb->Histogram() == &b && sb->State().request.peaks.empty(), "layer B starts independently");
  sb->SetRange(95, 115); sb->AddPeak(110); sb->Fit();
  Check(sb->State().hasResult, "B has a result before overwrite");
  const TString curveName = TString::Format("PhotoPeak_%p_total_fit", static_cast<void*>(&b));
  auto* oldCurve = left->GetListOfPrimitives()->FindObject(curveName);
  Check(oldCurve, "B has a fit curve before overwrite");
  Check(PhotoPeakFit::Start(&c, right), "activate other pad");
  Emit(canvas, left, &a, kMouseMotion, 100);
  Check(controller.ActiveSession()->Histogram() == &c, "hover cannot change active target");
  canvas.ProcessedEvent(kKeyPress, 'w', 0, &a);
  Check(c.GetNbinsX() == 500 && a.GetNbinsX() == 1000, "keyboard follows clicked target after hovering another pad");
  canvas.ProcessedEvent(kKeyPress, 'q', 0, &a);
  Seed(canvas, left, &a, 100);
  Check(controller.ActiveSession() == sb && sb->State().request.peaks.size() == 1,
    "first click restores last layer without adding a seed");
  Check(controller.SelectLayer(1), "restore layer A");
  Check(sa->State().request.peaks.size() == 1 && sa->State().request.fitLow == 90, "A request preserved");
  if(gui) Check(std::string(FindWidget<TGTextEntry>(panel, 10)->GetText()) == "unfinished", "invalid raw draft restored");
  Check(controller.SelectLayer(2), "return to B for overwrite");
  if(gui) panel->ProcessMessage(MK_MSG(kC_COMMAND, kCM_BUTTON), 801, 0);
  else Check(controller.CopyPrevious(), "copy previous settings");
  Check(sb->State().request.peaks.front().centroid.value == 100 && !sb->State().hasResult,
    "overwrite replaces B seeds and invalidates results");
  Check(sa->State().request.peaks.front().centroid.value == 100, "overwrite leaves source intact");
  Check(!left->GetListOfPrimitives()->FindObject(curveName), "overwrite removes B old curve");
  if(gui) {
    Check(std::string(FindWidget<TGTextEntry>(panel, 10)->GetText()) == "unfinished", "overwrite copies invalid source draft");
    panel->ProcessMessage(MK_MSG(kC_COMMAND, kCM_BUTTON), 802, 0);
    Check(std::string(FindWidget<TGTextEntry>(panel, 10)->GetText()) == "95", "undo restores target editor");
  } else Check(sb->UndoCopy(), "undo copied settings");
  Check(sb->State().request.peaks.front().centroid.value == 110, "undo restores target seeds");
  Check(sb->State().hasResult && left->GetListOfPrimitives()->FindObject(curveName) == oldCurve,
    "undo restores the original result and exact drawing");
  if(gui) {
    int mapped = 0;
    TIter windows(gClient->GetListOfWindows());
    while(auto* object = windows()) {
      auto* candidate = dynamic_cast<PhotoPeakControlWindow*>(object);
      if(candidate && candidate->IsMapped()) { ++mapped; Check(candidate == panel, "same native panel reused"); }
    }
    Check(mapped == 1, "only one control panel is mapped");
  }
  Check(PhotoPeakFit::Start(&c, right), "activate disappearing target");
  Check(controller.CopyPrevious(), "copy settings to narrower target");
  auto* sc = controller.ActiveSession();
  Check(sc->State().request.fitHigh == 115, "out-of-range settings remain unchanged");
  sc->Fit();
  Check(!sc->State().hasResult && sc->State().status.find("Fit range") != std::string::npos,
    "invalid copied range blocks fitting with a diagnostic");
  right->GetListOfPrimitives()->Remove(&c); controller.Poll();
  Check(PhotoPeakFit::Start(&a, left), "recover editor after active drawing removal");
  if(gui) Check(std::string(FindWidget<TGTextEntry>(panel, 10)->GetText()) == "unfinished", "other drafts survive active target removal");
  left->GetListOfPrimitives()->Remove(&b);
  controller.Poll();
  int markerCount = 0;
  TIter remaining(left->GetListOfPrimitives());
  while(auto* object = remaining()) markerCount += object->GetUniqueID() == 0x50500001;
  Check(markerCount == 3, "removing inactive B cannot clear active A markers");
  if(gui) {
    panel->Show();
    std::unique_ptr<TImage> image(TImage::Create());
    image->FromWindow(panel->GetId());
    image->WriteImage("tests/shared-panel-native.png");
    canvas.SaveAs("tests/shared-targets-native.png");
  }
  PhotoPeakFit::StopAll();
}

}

// ============== main ==============
// Purpose: Test shared targets. Inputs: Optional --gui. Outputs: Exit status.
int main(int argc, char** argv) {
  const bool gui = argc > 1 && std::string(argv[1]) == "--gui";
  int appArgc = gui ? 1 : 2;
  char batch[] = "-b";
  char* appArgv[] = {argv[0], batch, nullptr};
  TApplication app("SharedTargetsTests", &appArgc, appArgv);
  try {
    if(gui) Check(gClient && !gROOT->IsBatch(), "native desktop required");
    SharedTargets(gui);
    std::cout << "PASS shared PhotoPeakFit targets" << std::endl;
    return 0;
  } catch(const std::exception& error) {
    PhotoPeakFit::StopAll();
    std::cerr << "FAIL " << error.what() << std::endl;
    return 1;
  }
}

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
#include <TGButton.h>
#include <TImage.h>
#include <TPave.h>
#include <Buttons.h>
#include <KeySymbols.h>
#include <TTimer.h>
#include <TContextMenu.h>
#include <TRootContextMenu.h>
#include <TRootDialog.h>
#include <TVirtualX.h>
#include <vector>
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
// ============== Markers ==============
// Purpose: Count plugin markers. Inputs: Pad. Outputs: Count.
int Markers(TVirtualPad* pad) {
  int count = 0; TIter next(pad->GetListOfPrimitives());
  while(auto* object = next()) count += object->GetUniqueID() == 0x50500001;
  return count;
}
class AxisDialogResponse : public TTimer {
public:
  // ============== AxisDialogResponse::AxisDialogResponse ==============
  // Purpose: Respond to the real ROOT dialog from its nested event loop.
  // Inputs: Canvas and cancel flag. Outputs: Scheduled test response.
  AxisDialogResponse(TCanvas& canvas, bool cancel, bool apply = false)
    : TTimer(25, kTRUE), fCanvas(canvas), fCancel(cancel), fApply(apply) {
    TurnOn();
  }

  // ============== AxisDialogResponse::Notify ==============
  // Purpose: Fill the native SetRangeUser inputs and press OK or Cancel.
  // Inputs: Open context-menu dialog. Outputs: Recorded dialog identity and closed window.
  Bool_t Notify() override {
    auto* context = fCanvas.GetContextMenu();
    auto* menu = dynamic_cast<TRootContextMenu*>(context->GetContextMenuImp());
    auto* dialog = menu ? menu->GetDialog() : nullptr;
    if(!dialog) {
      Reset();
      return kTRUE;
    }
    TurnOff();
    opened = true;
    axis = context->GetCalledObject();
    std::vector<TGTextEntry*> fields;
    TIter children(dialog->GetList());
    while(auto* element = static_cast<TGFrameElement*>(children())) {
      if(auto* field = dynamic_cast<TGTextEntry*>(element->fFrame))
        fields.push_back(field);
    }
    validFields = fields.size() == 2;
    if(validFields) {
      fields[0]->SetText(fCancel ? "70" : "92", false);
      fields[1]->SetText(fCancel ? "140" : "118", false);
    }
    if(fApply && validFields) {
      menu->ProcessMessage(MK_MSG(kC_COMMAND, kCM_BUTTON), 2, 0);
      menu->ProcessMessage(MK_MSG(kC_COMMAND, kCM_BUTTON), 3, 0);
    } else menu->ProcessMessage(MK_MSG(kC_COMMAND, kCM_BUTTON), fCancel || !validFields ? 3 : 1, 0);
    return kFALSE;
  }

  bool opened = false;
  bool validFields = false;
  TObject* axis = nullptr;

private:
  TCanvas& fCanvas;
  bool fCancel;
  bool fApply;
};

// ============== AxisRangeShortcut ==============
// Purpose: Verify Shift+E opens the same native range dialog used by GRSISort.
// Inputs: Native GUI application. Outputs: Canvas-wide OK/Apply and working cancellation.
void AxisRangeShortcut() {
  TCanvas canvas("axisShortcut", "axisShortcut", 1000, 600);
  canvas.Divide(2, 1);
  TH1D first("axisFirst", "axisFirst", 1000, 0, 200);
  TH1D second("axisSecond", "axisSecond", 1000, 0, 200);
  FillSpectrum(first);
  FillSpectrum(second);
  auto* left = canvas.cd(1);
  first.Draw();
  auto* right = canvas.cd(2);
  second.Draw();
  canvas.Update();
  Check(PhotoPeakFit::Start(&first, left) && PhotoPeakFit::Start(&second, right), "axis shortcut sessions");
  Seed(canvas, left, &first, 100);
  Seed(canvas, left, &first, 100);
  canvas.ProcessedEvent(kKeyPress, 'f', 0, &first);
  const TString totalName = TString::Format("PhotoPeak_%p_total_fit", static_cast<void*>(&first));
  auto* curve = left->GetListOfPrimitives()->FindObject(totalName);
  Check(curve, "fit curve exists before axis zoom");
  Event_t event = {};
  event.fType = kGKeyPress;
  event.fWindow = gVirtualX->GetWindowID(canvas.GetCanvasID());
  event.fCode = gVirtualX->KeysymToKeycode(kKey_e);
  event.fState = kKeyShiftMask;
  {
    AxisDialogResponse response(canvas, false);
    gClient->HandleEvent(&event);
    Check(response.opened && response.validFields && response.axis != nullptr,
      "native Shift+E opens the target X-axis SetRangeUser dialog");
  }
  Check(std::fabs(first.GetXaxis()->GetBinLowEdge(first.GetXaxis()->GetFirst()) - 92) < 0.21 &&
    std::fabs(first.GetXaxis()->GetBinUpEdge(first.GetXaxis()->GetLast()) - 118) < 0.21,
    "dialog OK applies the requested X-axis view");
  Check(second.GetXaxis()->GetFirst() == first.GetXaxis()->GetFirst() && second.GetXaxis()->GetLast() == first.GetXaxis()->GetLast() &&
    left->GetListOfPrimitives()->FindObject(totalName) == curve && Markers(left) == 3,
    "axis dialog synchronizes pads and preserves fitted curves and peak selection");
  second.GetXaxis()->SetRange(0, 0);
  {
    AxisDialogResponse response(canvas, false, true);
    gClient->HandleEvent(&event);
    Check(response.opened && second.GetXaxis()->GetFirst() == first.GetXaxis()->GetFirst() &&
      second.GetXaxis()->GetLast() == first.GetXaxis()->GetLast(),
      "Apply with unchanged active range still synchronizes other pads");
  }
  second.GetXaxis()->SetRangeUser(70, 140);
  const int otherFirst = second.GetXaxis()->GetFirst();
  const int otherLast = second.GetXaxis()->GetLast();
  const int firstBin = first.GetXaxis()->GetFirst();
  const int lastBin = first.GetXaxis()->GetLast();
  event.fCode = gVirtualX->KeysymToKeycode(kKey_E);
  {
    AxisDialogResponse response(canvas, true);
    gClient->HandleEvent(&event);
    Check(response.opened && first.GetXaxis()->GetFirst() == firstBin && first.GetXaxis()->GetLast() == lastBin,
      "native range dialog Cancel preserves the view");
    Check(second.GetXaxis()->GetFirst() == otherFirst && second.GetXaxis()->GetLast() == otherLast,
      "Cancel cannot synchronize or alter another pad");
  }
  PhotoPeakFit::StopAll();
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
  TCanvas unrelated("unrelatedRange", "Unrelated", 300, 250);
  TH1D unrelatedHist("unrelatedHist", "Other", 200, 0, 200);
  unrelatedHist.SetDirectory(nullptr); unrelatedHist.Draw(); unrelated.Update();
  controller.ApplyCanvasRange(&canvas, 30, 60);
  for(auto* histogram : {&a, &b, &c})
    Check(std::abs(histogram->GetXaxis()->GetBinLowEdge(histogram->GetXaxis()->GetFirst()) - 30) < 0.21 &&
      std::abs(histogram->GetXaxis()->GetBinUpEdge(histogram->GetXaxis()->GetLast()) - 60) < 0.21,
      "canvas range applies to inactive pads and layers");
  Check(!unrelatedHist.GetXaxis()->TestBit(TAxis::kAxisRange), "other canvas is untouched");
  for(auto* histogram : {&a, &b, &c}) histogram->GetXaxis()->SetRange(0, 0);
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
    Check(!FindWidget<TGTextButton>(panel, 802), "overwrite undo button is removed");
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
  a.GetXaxis()->SetRangeUser(85, 130);
  a.SetMinimum(1); a.SetMaximum(1500);
  b.GetXaxis()->SetRange(0, 0);
  Check(controller.SelectLayer(2), "return to B for overwrite");
  if(gui) panel->ProcessMessage(MK_MSG(kC_COMMAND, kCM_BUTTON), 801, 0);
  else Check(controller.CopyPrevious(), "copy previous settings");
  Check(sb->State().request.peaks.front().centroid.value == 100 && !sb->State().hasResult,
    "overwrite replaces B seeds and invalidates results");
  Check(b.GetXaxis()->GetFirst() == a.GetXaxis()->GetFirst() &&
    b.GetXaxis()->GetLast() == a.GetXaxis()->GetLast() && b.GetMinimumStored() == 1 && b.GetMaximumStored() == 1500,
    "overwrite copies X zoom and Y display range");
  Check(sa->State().request.peaks.front().centroid.value == 100, "overwrite leaves source intact");
  Check(!left->GetListOfPrimitives()->FindObject(curveName), "overwrite removes B old curve");
  if(gui) Check(std::string(FindWidget<TGTextEntry>(panel, 10)->GetText()) == "unfinished", "overwrite copies invalid source draft");
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
  Check(sc->State().request.fitHigh == 120, "out-of-range settings remain unchanged");
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
    if(gui) AxisRangeShortcut();
    std::cout << "PASS shared PhotoPeakFit targets" << std::endl;
    return 0;
  } catch(const std::exception& error) {
    PhotoPeakFit::StopAll();
    std::cerr << "FAIL " << error.what() << std::endl;
    return 1;
  }
}

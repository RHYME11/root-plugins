#include <TCanvas.h>
#include <TH1D.h>
#include <TSystem.h>

// ============== LoadSmoke ==============
// Purpose: Check the generated loader and interpreter-visible public API.
// Inputs: A previously executed LoadPhotoPeakFit.C.
// Outputs: Successful start, cleanup, and exit through the loaded library.
void LoadSmoke() {
  TH1D hist("loaderSmoke", "loaderSmoke", 100, 0, 100);
  hist.SetDirectory(nullptr);
  TCanvas canvas("loaderCanvas", "loaderCanvas");
  hist.Draw();
  if(!PhotoPeakFit::Start(&hist, &canvas) || !PhotoPeakFit::Clean(&canvas) ||
     !PhotoPeakFit::Stop(&canvas)) {
    Error("LoadSmoke", "%s", PhotoPeakFit::LastError());
    gSystem->Exit(1);
  }
  Printf("PASS PhotoPeakFit interpreted loader");
}

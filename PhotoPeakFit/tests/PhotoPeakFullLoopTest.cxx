#include <cstdio>
#include <cstdlib>
#include <string>

#include <TCanvas.h>
#include <TFile.h>
#include <TH1.h>
#include <TROOT.h>
#include <TSystem.h>

#include <Plugin/GPluginManager.h>

namespace {

// ============== Fail ==============
// Purpose: Report one full-loop test failure.
// Inputs: Failure message.
// Outputs: Process failure code.
int Fail(const char* message) {
  std::fprintf(stderr, "PhotoPeakFullLoopTest: %s\n", message);
  return 1;
}

// ============== LibraryIsLoaded ==============
// Purpose: Check ROOT's dynamic-library list for the PhotoPeak plugin.
// Inputs: None.
// Outputs: True when the plugin library has been loaded.
bool LibraryIsLoaded() {
  const char* libraries = gSystem->GetLibraries();
  return libraries &&
    std::string(libraries).find(PHOTOPEAK_PLUGIN_LIBRARY) != std::string::npos;
}

} // namespace

// ============== main ==============
// Purpose: Validate manifest discovery, lazy loading, and action execution.
// Inputs: None.
// Outputs: Process success or failure code.
int main() {
  gROOT->SetBatch(true);
  setenv("GROOT_PLUGIN_PATH", PHOTOPEAK_PLUGIN_DIRECTORY, 1);
  setenv("HOME", "/tmp/photopeak-full-loop-home", 1);
  unsetenv("GSYS");

  TFile file(PHOTOPEAK_TEST_DATA, "READ");
  auto* hist = file.Get<TH1>("ParticleGates/Er/GammaEfficiency_Er");
  if(!hist)
    return Fail("test histogram was not found");
  TCanvas canvas("full_loop_test", "full_loop_test", 800, 600);
  hist->Draw();
  canvas.Update();

  GPluginManager& manager = GPluginManager::Get();
  manager.SetContextProvider([&canvas, hist]() {
    return GPluginContext{&canvas, &canvas, hist, hist};
  });
  manager.Initialize();
  bool found = false;
  for(const GPluginAction& action : manager.Actions()) {
    if(action.id == "photopeakfit.fit")
      found = true;
  }
  if(!found)
    return Fail("manifest action was not discovered");
  if(LibraryIsLoaded())
    return Fail("plugin library loaded before action execution");
  if(!manager.ExecuteAction("photopeakfit.fit"))
    return Fail("manager action execution failed");
  if(!LibraryIsLoaded())
    return Fail("plugin library was not loaded on demand");
  return 0;
}

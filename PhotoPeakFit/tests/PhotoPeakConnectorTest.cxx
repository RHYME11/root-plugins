#include <cstdint>
#include <cstdio>
#include <string>

#include <TCanvas.h>
#include <TFile.h>
#include <TH1.h>
#include <TList.h>
#include <TObject.h>
#include <TROOT.h>

#include <Plugin/GPlugin.h>
#include <Plugin/GPluginHost.h>
#include <Plugin/GPluginVersion.h>

extern "C" std::uint32_t GrootPluginApiVersion();
extern "C" GPlugin* GrootCreatePlugin(GPluginHost* host);
extern "C" void GrootDestroyPlugin(GPlugin* plugin);

namespace {

class FakeHost : public GPluginHost {
  public:
    void SetStatusMessage(const char* message) override {
      status = message ? message : "";
    }

    std::string status;
};

// ============== Fail ==============
// Purpose: Report one connector-test failure.
// Inputs: Failure message.
// Outputs: Process failure code.
int Fail(const char* message) {
  std::fprintf(stderr, "PhotoPeakConnectorTest: %s\n", message);
  return 1;
}

// ============== HasFitCurves ==============
// Purpose: Check that the connector drew total and background functions.
// Inputs: Canvas containing the tested histogram.
// Outputs: True when both plugin-owned curves are present.
bool HasFitCurves(TCanvas& canvas) {
  int count = 0;
  TIter next(canvas.GetListOfPrimitives());
  while(TObject* object = next()) {
    if(std::string(object->GetName()).find("PhotoPeak_") == 0)
      ++count;
  }
  return count >= 2;
}

} // namespace

// ============== main ==============
// Purpose: Validate the Groot connector with a fake host and real context.
// Inputs: None.
// Outputs: Process success or failure code.
int main() {
  gROOT->SetBatch(true);
  if(GrootPluginApiVersion() != kGPluginApiVersion)
    return Fail("connector API version is wrong");

  FakeHost host;
  GPlugin* plugin = GrootCreatePlugin(&host);
  if(!plugin)
    return Fail("connector factory returned null");

  GPluginContext empty;
  if(plugin->ExecuteAction("photopeakfit.fit", empty)) {
    GrootDestroyPlugin(plugin);
    return Fail("empty context unexpectedly succeeded");
  }
  if(std::string(plugin->LastError()).empty()) {
    GrootDestroyPlugin(plugin);
    return Fail("empty context did not report an error");
  }

  TFile file(PHOTOPEAK_TEST_DATA, "READ");
  auto* hist = file.Get<TH1>("ParticleGates/Er/GammaEfficiency_Er");
  if(!hist) {
    GrootDestroyPlugin(plugin);
    return Fail("test histogram was not found");
  }
  TCanvas canvas("connector_test", "connector_test", 800, 600);
  hist->Draw();
  canvas.Update();
  GPluginContext context{&canvas, &canvas, hist};
  const bool success = plugin->ExecuteAction("photopeakfit.fit", context);
  const bool statusOk = host.status == "PhotoPeakFit completed";
  const bool curvesOk = HasFitCurves(canvas);
  GrootDestroyPlugin(plugin);

  if(!success)
    return Fail("fit action failed");
  if(!statusOk)
    return Fail("success status was not delivered to the host");
  if(!curvesOk)
    return Fail("fit curves were not drawn");
  return 0;
}

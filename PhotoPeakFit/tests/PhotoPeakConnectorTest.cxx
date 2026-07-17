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

    bool ActivateSession(GPluginSession* session,
                         const GPluginContext& context) override {
      activeSession = session;
      activeContext = context;
      return session && context.pad;
    }

    void DeactivateSession(GPluginSession* session) override {
      if(activeSession == session)
        activeSession = nullptr;
    }

    std::string status;
    GPluginSession* activeSession = nullptr;
    GPluginContext activeContext;
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
  GPluginContext context{&canvas, &canvas, hist, hist};
  const bool success = plugin->ExecuteAction("photopeakfit.fit", context);
  const bool statusOk = host.status == "PhotoPeak fit mode activated";
  const bool curvesOk = host.activeSession != nullptr;

  TCanvas ambiguousCanvas("ambiguous_test", "ambiguous_test", 800, 600);
  auto* firstCopy = dynamic_cast<TH1*>(hist->Clone("first_copy"));
  auto* secondCopy = dynamic_cast<TH1*>(hist->Clone("second_copy"));
  firstCopy->SetDirectory(nullptr);
  secondCopy->SetDirectory(nullptr);
  firstCopy->Draw();
  secondCopy->Draw("same");
  ambiguousCanvas.Update();
  GPluginContext ambiguous{&ambiguousCanvas, &ambiguousCanvas, nullptr, nullptr};
  const bool rejectedAmbiguous =
    !plugin->ExecuteAction("photopeakfit.fit", ambiguous);
  ambiguous.selected = secondCopy;
  ambiguous.target = secondCopy;
  const bool acceptedSelection =
    plugin->ExecuteAction("photopeakfit.fit", ambiguous);
  GrootDestroyPlugin(plugin);

  if(!success)
    return Fail("fit action failed");
  if(!statusOk)
    return Fail("success status was not delivered to the host");
  if(!curvesOk)
    return Fail("fit curves were not drawn");
  if(!rejectedAmbiguous)
    return Fail("ambiguous pad did not require explicit histogram selection");
  if(!acceptedSelection)
    return Fail("explicitly selected histogram was not accepted");
  return 0;
}

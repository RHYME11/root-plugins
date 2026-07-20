#include <cstdint>
#include <cstdio>
#include <string>

#include <Buttons.h>
#include <GMarker.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TF1.h>
#include <TH1.h>
#include <TList.h>
#include <TObject.h>
#include <TROOT.h>
#include <TString.h>

#include <Plugin/GPlugin.h>
#include <Plugin/GPluginEvent.h>
#include <Plugin/GPluginHost.h>
#include <Plugin/GPluginSession.h>
#include <Plugin/GPluginVersion.h>
#include "PhotoPeakGrootEventAdapter.h"

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

// ============== CountPhotoPeakArtifacts ==============
// Purpose: Count PhotoPeak-owned marker and named drawing primitives.
// Inputs: Canvas containing the tested histogram.
// Outputs: Number of matching plugin artifacts.
int CountPhotoPeakArtifacts(TCanvas& canvas) {
  int count = 0;
  TIter next(canvas.GetListOfPrimitives());
  while(TObject* object = next()) {
    if(object->GetUniqueID() == 0x50500001 ||
       std::string(object->GetName()).find("PhotoPeak_") == 0)
      ++count;
  }
  return count;
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
  auto* grootMarker = new GMarker;
  grootMarker->AddTo(hist, 180.0, 0.0, true);
  if(GMarker::Get(hist, GMarkerType::kAll).empty()) {
    GrootDestroyPlugin(plugin);
    return Fail("Groot marker test setup failed");
  }
  GPluginContext context{&canvas, &canvas, hist, hist};
  const bool success = plugin->ExecuteAction("photopeakfit.fit", context);
  const bool statusOk = host.status == "PhotoPeak fit mode activated";
  const bool sessionOk = host.activeSession != nullptr;
  const bool grootMarkersRemoved =
    GMarker::Get(hist, GMarkerType::kAll).empty();

  GPluginEvent event;
  event.context = context;
  event.type = kButton1Down;
  event.x = 175.0;
  event.px = canvas.XtoAbsPixel(event.x);
  if(host.activeSession)
    host.activeSession->ObserveEvent(event);
  canvas.cd();
  auto* namedArtifact = new TF1(
    TString::Format("PhotoPeak_%p_test_artifact", static_cast<void*>(hist)),
    "0", 170.0, 180.0);
  namedArtifact->Draw("same");
  const int retainedCount = CountPhotoPeakArtifacts(canvas);
  GPluginSession* retainedSession = host.activeSession;
  auto* retainedAdapter =
    dynamic_cast<PhotoPeakGrootEventAdapter*>(retainedSession);
  if(retainedAdapter)
    retainedAdapter->Suspend();
  const bool retainedOnExit =
    CountPhotoPeakArtifacts(canvas) == retainedCount && retainedCount > 0;
  const bool resumed = plugin->ExecuteAction("photopeakfit.fit", context) &&
    host.activeSession == retainedSession &&
    host.status == "PhotoPeak fit mode resumed";
  if(retainedAdapter)
    retainedAdapter->Suspend();
  const bool cleanupSucceeded = plugin->CleanArtifacts(context) &&
    CountPhotoPeakArtifacts(canvas) == 0;

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
  if(!sessionOk)
    return Fail("fit session was not activated");
  if(!grootMarkersRemoved)
    return Fail("entering fit mode did not delete existing Groot markers");
  if(!retainedOnExit)
    return Fail("exiting fit mode did not retain PhotoPeak artifacts");
  if(!resumed)
    return Fail("re-entering fit mode did not resume the retained session");
  if(!cleanupSucceeded)
    return Fail("plugin cleanup did not remove all PhotoPeak artifacts");
  if(!rejectedAmbiguous)
    return Fail("ambiguous pad did not require explicit histogram selection");
  if(!acceptedSelection)
    return Fail("explicitly selected histogram was not accepted");
  return 0;
}

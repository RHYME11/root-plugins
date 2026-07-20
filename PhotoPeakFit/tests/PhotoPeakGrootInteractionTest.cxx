#include <cstdio>
#include <string>

#include <TCanvas.h>
#include <Buttons.h>
#include <TAxis.h>
#include <TH1D.h>
#include <TList.h>
#include <TROOT.h>

#include <Plugin/GPluginContext.h>
#include <Plugin/GPluginEvent.h>
#include <Plugin/GPluginHost.h>

#include "PhotoPeakGrootContextResolver.h"
#include "PhotoPeakGrootEventAdapter.h"

namespace {

class FakeHost : public GPluginHost {
  public:
    void SetStatusMessage(const char* message) override {
      status = message ? message : "";
    }

    bool ActivateSession(GPluginSession* session,
                         const GPluginContext&) override {
      active = session;
      return session != nullptr;
    }

    void DeactivateSession(GPluginSession* session) override {
      if(active == session)
        active = nullptr;
    }

    GPluginSession* active = nullptr;
    std::string status;
};

// ============== Fail ==============
// Purpose: Report one Groot-interaction test failure.
// Inputs: Failure description.
// Outputs: Non-zero process result.
int Fail(const char* message) {
  std::fprintf(stderr, "PhotoPeakGrootInteractionTest: %s\n", message);
  return 1;
}

// ============== CountPhotoPeakMarkers ==============
// Purpose: Count interactive PhotoPeak primitives in one pad.
// Inputs: Canvas primitive list.
// Outputs: Number of marker-owned objects.
int CountPhotoPeakMarkers(TCanvas& canvas) {
  int count = 0;
  TIter next(canvas.GetListOfPrimitives());
  while(TObject* object = next()) {
    if(object->GetUniqueID() == 0x50500001)
      ++count;
  }
  return count;
}

} // namespace

// ============== main ==============
// Purpose: Validate target resolution and connector lifecycle observation.
// Inputs: None.
// Outputs: Process success or failure.
int main() {
  gROOT->SetBatch(true);
  TCanvas canvas("groot_interaction_test", "groot_interaction_test", 600, 400);
  TH1D histogram("groot_interaction_hist", "groot_interaction_hist",
                 100, 0.0, 100.0);
  histogram.Draw();
  canvas.Update();

  GPluginContext context{&canvas, &canvas, &histogram, &histogram};
  std::string error;
  if(ResolvePhotoPeakHistogram(context, error) != &histogram)
    return Fail("selected histogram was not resolved");

  FakeHost host;
  PhotoPeakGrootEventAdapter adapter(&host, &canvas, &canvas, &histogram);
  if(std::string(adapter.SessionId()) != "photopeakfit.session")
    return Fail("connector session ID is unstable");
  if(!host.ActivateSession(&adapter, context))
    return Fail("fake host activation failed");

  GPluginEvent event;
  event.context = context;
  event.type = kButton3Down;
  adapter.ObserveEvent(event);
  if(host.active != &adapter)
    return Fail("unhandled ROOT event closed the PhotoPeak session");

  event.type = kButton1Down;
  event.x = 25.0;
  event.context.selected = histogram.GetXaxis();
  adapter.ObserveEvent(event);
  if(CountPhotoPeakMarkers(canvas) != 0)
    return Fail("axis click leaked into PhotoPeak marker interaction");
  event.context.selected = &histogram;
  adapter.ObserveEvent(event);
  if(CountPhotoPeakMarkers(canvas) != 2)
    return Fail("histogram click did not reach PhotoPeak interaction");

  canvas.GetListOfPrimitives()->Remove(&histogram);
  adapter.ObserveEvent(event);
  if(host.active || host.status.find("target was removed") == std::string::npos)
    return Fail("removed target did not safely close the connector session");
  return 0;
}

#include <algorithm>
#include <memory>
#include <string>
#include <GMarker.h>
#include <TH1.h>
#include <TVirtualPad.h>

#include <Plugin/GPlugin.h>
#include <Plugin/GPluginHost.h>
#include <Plugin/GPluginVersion.h>

#include "PhotoPeakGrootEventAdapter.h"
#include "PhotoPeakGrootContextResolver.h"

namespace {

class PhotoPeakGrootPlugin : public GPlugin {
  public:
    explicit PhotoPeakGrootPlugin(GPluginHost* host) : fHost(host) {
    }

    bool ExecuteAction(const char* actionId,
                       const GPluginContext& context) override {
      fError.clear();
      if(!actionId || std::string(actionId) != "photopeakfit.fit") {
        fError = "unsupported PhotoPeakFit action";
        return false;
      }
      if(!context.canvas || !context.pad) {
        fError = "select a canvas pad first";
        return false;
      }
      fSessions.erase(std::remove_if(fSessions.begin(), fSessions.end(),
        [](const std::unique_ptr<PhotoPeakGrootEventAdapter>& session) {
          return session->Target() == nullptr;
        }), fSessions.end());
      const auto active = std::find_if(fSessions.begin(), fSessions.end(),
        [&](const std::unique_ptr<PhotoPeakGrootEventAdapter>& session) {
          return !session->IsClosed() && session->Pad() == context.pad;
        });
      if(active != fSessions.end()) {
        (*active)->RaiseWindow();
        return true;
      }
      TH1* histogram = ResolvePhotoPeakHistogram(context, fError);
      if(!histogram)
        return false;
      GPluginContext sessionContext = context;
      sessionContext.target = histogram;
      auto session = std::make_unique<PhotoPeakGrootEventAdapter>(
        fHost, context.canvas, context.pad, histogram);
      if(!fHost || !fHost->ActivateSession(session.get(), sessionContext)) {
        fError = "Groot refused the PhotoPeak pad session";
        return false;
      }
      GMarker::RemoveAll(histogram, true);
      fSessions.push_back(std::move(session));
      fHost->SetStatusMessage("PhotoPeak fit mode activated");
      return true;
    }

    bool CleanArtifacts(const GPluginContext& context) override {
      fError.clear();
      for(const auto& session : fSessions) {
        if(session->Pad() == context.pad && session->Target() == context.target)
          session->CleanArtifacts();
      }
      return true;
    }

    const char* LastError() const override {
      return fError.c_str();
    }

  private:
    GPluginHost* fHost;
    std::string fError;
    std::vector<std::unique_ptr<PhotoPeakGrootEventAdapter>> fSessions;
};

} // namespace

extern "C" std::uint32_t GrootPluginApiVersion() {
  return kGPluginApiVersion;
}

extern "C" GPlugin* GrootCreatePlugin(GPluginHost* host) {
  return new PhotoPeakGrootPlugin(host);
}

extern "C" void GrootDestroyPlugin(GPlugin* plugin) {
  delete plugin;
}

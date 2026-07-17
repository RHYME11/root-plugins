#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <TH1.h>
#include <TList.h>
#include <TObject.h>
#include <TVirtualPad.h>

#include <Plugin/GPlugin.h>
#include <Plugin/GPluginHost.h>
#include <Plugin/GPluginVersion.h>

#include "PhotoPeakGrootEventAdapter.h"

namespace {

// ============== FindHistogram ==============
// Purpose: Resolve one unambiguous one-dimensional histogram in a pad.
// Inputs: Groot action context and error output.
// Outputs: Selected or sole directly drawn TH1.
TH1* FindHistogram(const GPluginContext& context, std::string& error) {
  auto* selected = dynamic_cast<TH1*>(context.selected);
  const bool selectedInPad = selected && context.pad &&
    context.pad->GetListOfPrimitives() &&
    context.pad->GetListOfPrimitives()->FindObject(selected) == selected;
  if(selectedInPad && selected->GetDimension() == 1)
    return selected;
  if(!context.pad || !context.pad->GetListOfPrimitives()) {
    error = "select or draw one one-dimensional histogram first";
    return nullptr;
  }
  std::vector<TH1*> histograms;
  TIter next(context.pad->GetListOfPrimitives());
  while(TObject* object = next()) {
    auto* hist = dynamic_cast<TH1*>(object);
    if(hist && hist->GetDimension() == 1)
      histograms.push_back(hist);
  }
  if(histograms.size() == 1)
    return histograms.front();
  error = histograms.empty() ?
    "select or draw one one-dimensional histogram first" :
    "multiple histograms are drawn; click the target histogram first";
  return nullptr;
}

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
      const auto existing = std::find_if(fSessions.begin(), fSessions.end(),
        [&](const std::unique_ptr<PhotoPeakGrootEventAdapter>& session) {
          return !session->IsClosed() && session->Pad() == context.pad;
        });
      if(existing != fSessions.end()) {
        (*existing)->RaiseWindow();
        return true;
      }
      TH1* histogram = FindHistogram(context, fError);
      if(!histogram)
        return false;
      auto session = std::make_unique<PhotoPeakGrootEventAdapter>(
        fHost, context.canvas, context.pad, histogram);
      GPluginContext sessionContext = context;
      sessionContext.target = histogram;
      if(!fHost || !fHost->ActivateSession(session.get(), sessionContext)) {
        fError = "Groot refused the PhotoPeak pad session";
        return false;
      }
      fSessions.push_back(std::move(session));
      fHost->SetStatusMessage("PhotoPeak fit mode activated");
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

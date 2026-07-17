#include <PhotoPeakFit/PhotoPeakFitter.h>

#include <string>

#include <TH1.h>
#include <TList.h>
#include <TObject.h>
#include <TVirtualPad.h>

#include <Plugin/GPlugin.h>
#include <Plugin/GPluginHost.h>
#include <Plugin/GPluginVersion.h>

namespace {

// ============== FindHistogram ==============
// Purpose: Find the selected one-dimensional histogram for a plugin action.
// Inputs: Groot plugin context.
// Outputs: Selected or first directly drawn one-dimensional histogram.
TH1* FindHistogram(const GPluginContext& context) {
  auto* selected = dynamic_cast<TH1*>(context.selected);
  if(selected && selected->GetDimension() == 1)
    return selected;
  if(!context.pad || !context.pad->GetListOfPrimitives())
    return nullptr;

  TIter next(context.pad->GetListOfPrimitives());
  while(TObject* object = next()) {
    auto* hist = dynamic_cast<TH1*>(object);
    if(hist && hist->GetDimension() == 1)
      return hist;
  }
  return nullptr;
}

class PhotoPeakGrootPlugin : public GPlugin {
  public:
    explicit PhotoPeakGrootPlugin(GPluginHost* host) : fHost(host) {
    }

    // ============== ExecuteAction ==============
    // Purpose: Execute the minimal Groot single-photopeak action.
    // Inputs: Action identifier and current Groot context.
    // Outputs: True after a successful ROOT fit.
    bool ExecuteAction(const char* actionId,
                       const GPluginContext& context) override {
      fError.clear();
      if(!actionId || std::string(actionId) != "photopeakfit.fit") {
        fError = "unsupported PhotoPeakFit action";
        return false;
      }

      TH1* hist = FindHistogram(context);
      if(!hist) {
        fError = "select or draw one one-dimensional histogram first";
        return false;
      }
      if(context.pad)
        context.pad->cd();

      const PhotoPeakFitResult result =
        PhotoPeakFitter::Fit(hist, 175.0, 210.0, 191.75, context.pad, true);
      if(result.status != 0) {
        fError = "photopeak fit returned status " + std::to_string(result.status);
        return false;
      }
      if(fHost)
        fHost->SetStatusMessage("PhotoPeakFit completed");
      return true;
    }

    const char* LastError() const override {
      return fError.c_str();
    }

  private:
    GPluginHost* fHost;
    std::string fError;
};

} // namespace

// ============== GrootPluginApiVersion ==============
// Purpose: Report the Groot Plugin API required by this connector.
// Inputs: None.
// Outputs: Groot Plugin API version.
extern "C" std::uint32_t GrootPluginApiVersion() {
  return kGPluginApiVersion;
}

// ============== GrootCreatePlugin ==============
// Purpose: Create one PhotoPeakFit Groot connector instance.
// Inputs: Groot host interface.
// Outputs: New plugin instance owned by Groot.
extern "C" GPlugin* GrootCreatePlugin(GPluginHost* host) {
  return new PhotoPeakGrootPlugin(host);
}

// ============== GrootDestroyPlugin ==============
// Purpose: Destroy one PhotoPeakFit Groot connector instance.
// Inputs: Plugin previously created by GrootCreatePlugin.
// Outputs: Released plugin instance.
extern "C" void GrootDestroyPlugin(GPlugin* plugin) {
  delete plugin;
}

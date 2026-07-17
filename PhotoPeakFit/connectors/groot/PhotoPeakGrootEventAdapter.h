#ifndef __PHOTOPEAKGROOTEVENTADAPTER_H__
#define __PHOTOPEAKGROOTEVENTADAPTER_H__

#include <memory>

#include <Plugin/GPluginSession.h>

class GPluginHost;
class PhotoPeakSession;
class TCanvas;
class TH1;
class TVirtualPad;

class PhotoPeakGrootEventAdapter : public GPluginSession {
  public:
    PhotoPeakGrootEventAdapter(GPluginHost* host, TCanvas* canvas,
                              TVirtualPad* pad, TH1* histogram);
    ~PhotoPeakGrootEventAdapter() override;
    bool HandleEvent(const GPluginEvent& event) override;
    void Close() override;
    void RaiseWindow();
    TVirtualPad* Pad() const;
    bool IsClosed() const;

  private:
    GPluginHost* fHost;
    std::unique_ptr<PhotoPeakSession> fSession;
};

#endif

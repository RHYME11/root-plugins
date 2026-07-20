#ifndef __PHOTOPEAKSESSION_H__
#define __PHOTOPEAKSESSION_H__

#include <functional>
#include <memory>

#include <PhotoPeakFit/PhotoPeakSessionState.h>

class PhotoPeakControlWindow;
class PhotoPeakDrawController;
class PhotoPeakMarkerController;
class TCanvas;
class TH1;
class TVirtualPad;

class PhotoPeakSession {
  public:
    PhotoPeakSession(TCanvas* canvas, TVirtualPad* pad, TH1* histogram);
    ~PhotoPeakSession();

    bool HandleEvent(const PhotoPeakInputEvent& event);
    void Close();
    void Abandon();
    void Resume();
    void ExitMode();
    void RaiseWindow();
    void Fit();
    void Clean();
    void SetRange(double low, double high);
    void AddPeak(double centroid);
    void RemovePeak(std::size_t index);
    void ReplaceRequest(const PhotoPeakFitRequest& request);

    TCanvas* Canvas() const { return fCanvas; }
    TVirtualPad* Pad() const { return fPad; }
    TH1* Histogram() const { return fHistogram; }
    const PhotoPeakSessionState& State() const { return fState; }
    bool IsClosed() const { return fState.closed; }
    void SetExitCallback(std::function<void()> callback);

  private:
    void RefreshMarkers();
    void RefreshGui();
    void Rebin(bool undo);
    void Pan(int direction);
    void ShowBackground();

    TCanvas* fCanvas;
    TVirtualPad* fPad;
    TH1* fHistogram;
    std::unique_ptr<TH1> fOriginalHistogram;
    PhotoPeakSessionState fState;
    std::unique_ptr<PhotoPeakControlWindow> fWindow;
    std::unique_ptr<PhotoPeakMarkerController> fMarkers;
    std::unique_ptr<PhotoPeakDrawController> fDrawing;
    std::function<void()> fExitCallback;
    int fRangeClicks;
    int fDraggingRange;
    int fDraggingPeak;
    bool fPeakMoved;
    bool fPeakWasExisting;
};

#endif

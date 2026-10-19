#ifndef __PHOTOPEAKSESSION_H__
#define __PHOTOPEAKSESSION_H__

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
    PhotoPeakSession(TCanvas* canvas, TVirtualPad* pad, TH1* histogram, bool createWindow = true);
    ~PhotoPeakSession();

    std::unique_ptr<PhotoPeakControlWindow> TakeWindow();
    void AdoptWindow(std::unique_ptr<PhotoPeakControlWindow> window);
    bool CopySettings(const PhotoPeakSession& source);
    bool HandleEvent(const PhotoPeakInputEvent& event);
    void Abandon();
    void ExitMode();
    void RaiseWindow();
    void Fit();
    void Clean();
    void SetRange(double low, double high);
    void AddPeak(double centroid);
    bool ReplaceRequest(const PhotoPeakFitRequest& request);
    void SelectPeak(std::uint64_t id);
    void RemoveSelectedPeak();
    void SetInteraction(PhotoPeakInteraction mode);
    void Rebin(bool undo);
    void Pan(int direction);
    void Unzoom();
    void RefreshView();
    bool RefreshSnapshot(const TH1& snapshot);
    void InvalidateSource(const std::string& message);
    void ShowBackground();
    void SetStatus(const std::string& message);

    TCanvas* Canvas() const { return fCanvas; }
    TVirtualPad* Pad() const { return fPad; }
    TH1* Histogram() const { return fHistogram; }
    const PhotoPeakSessionState& State() const { return fState; }
    bool IsClosed() const { return fState.closed; }
    bool ValidateTarget();
    void DetachTarget();

  private:
    void RefreshMarkers();
    void RefreshGui();
    bool CommitPending();
    bool ValidateRequest(const PhotoPeakFitRequest& request);
    void Changed();
    void SortPeaks();
    void CancelDrag();

    bool fManaged = false;
    TCanvas* fCanvas;
    TVirtualPad* fPad;
    TH1* fHistogram;
    std::unique_ptr<TH1> fOriginalHistogram;
    PhotoPeakSessionState fState;
    std::unique_ptr<PhotoPeakControlWindow> fWindow;
    std::unique_ptr<PhotoPeakMarkerController> fMarkers;
    std::unique_ptr<PhotoPeakDrawController> fDrawing;
    std::uint64_t fNextPeakId = 1;
    int fRangeClicks = 0;
    int fDraggingRange = -1;
    std::uint64_t fDraggingPeak = 0;
    int fDragStart = 0;
};

#endif

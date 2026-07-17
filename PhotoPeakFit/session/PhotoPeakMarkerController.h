#ifndef __PHOTOPEAKMARKERCONTROLLER_H__
#define __PHOTOPEAKMARKERCONTROLLER_H__

struct PhotoPeakSessionState;
class TH1;
class TVirtualPad;

class PhotoPeakMarkerController {
  public:
    PhotoPeakMarkerController(TVirtualPad* pad, TH1* histogram);
    void Refresh(const PhotoPeakSessionState& state);
    void Clear();

  private:
    TVirtualPad* fPad;
    TH1* fHistogram;
};

#endif

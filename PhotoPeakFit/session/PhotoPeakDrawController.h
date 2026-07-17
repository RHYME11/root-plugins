#ifndef __PHOTOPEAKDRAWCONTROLLER_H__
#define __PHOTOPEAKDRAWCONTROLLER_H__

class TH1;
class TVirtualPad;

class PhotoPeakDrawController {
  public:
    PhotoPeakDrawController(TVirtualPad* pad, TH1* histogram);
    void CleanFitObjects();

  private:
    TVirtualPad* fPad;
    TH1* fHistogram;
};

#endif

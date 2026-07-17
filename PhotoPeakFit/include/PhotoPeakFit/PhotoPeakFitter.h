#ifndef __PHOTOPEAKFITTER_H__
#define __PHOTOPEAKFITTER_H__

#include <PhotoPeakFit/PhotoPeakFitResult.h>

class TH1;
class TVirtualPad;

class PhotoPeakFitter {
  public:
    static PhotoPeakFitResult Fit(TH1* hist,
                                  double fitLow,
                                  double fitHigh,
                                  double peak0,
                                  TVirtualPad* pad = nullptr,
                                  bool draw = true);
};

#endif

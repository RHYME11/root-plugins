#ifndef __PHOTOPEAKFITTER_H__
#define __PHOTOPEAKFITTER_H__

#include <PhotoPeakFit/PhotoPeakFitResult.h>
#include <PhotoPeakFit/PhotoPeakFitRequest.h>

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
    static PhotoPeakFitResult Fit(TH1* hist,
                                  double fitLow,
                                  double fitHigh,
                                  double peak0,
                                  const PhotoPeakFitConfig& config,
                                  TVirtualPad* pad = nullptr,
                                  bool draw = true);
    static PhotoPeakFitResult Fit(TH1* hist,
                                  const PhotoPeakFitRequest& request,
                                  TVirtualPad* pad = nullptr,
                                  bool draw = true);
};

#endif

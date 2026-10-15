#ifndef __PHOTOPEAKBACKGROUND_H__
#define __PHOTOPEAKBACKGROUND_H__

#include <memory>
#include <string>

#include <PhotoPeakFit/PhotoPeakFitConfig.h>

class TF1;
class TH1;
class TVirtualPad;

struct PhotoPeakBackgroundEstimate {
  std::shared_ptr<TH1> histogram;
  double low = 0.0;
  double high = 0.0;
  std::string error;
};

PhotoPeakBackgroundEstimate EstimatePhotoPeakBackground(TH1* histogram,
  const PhotoPeakFitConfig& config, double fitLow, double fitHigh);
double PhotoPeakBackgroundAt(const PhotoPeakBackgroundEstimate& estimate, double x);
void RemovePhotoPeakBackground(TVirtualPad* pad, TH1* histogram);
void DrawPhotoPeakBackground(TVirtualPad* pad, TH1* histogram,
  const PhotoPeakBackgroundEstimate& estimate);
TF1* RestorePhotoPeakBackground(TF1* residual, const PhotoPeakBackgroundEstimate& estimate);

class PhotoPeakFitInput {
  public:
    PhotoPeakFitInput(TH1* original, const PhotoPeakFitConfig& config,
      double fitLow, double fitHigh);
    ~PhotoPeakFitInput();
    PhotoPeakFitInput(const PhotoPeakFitInput&) = delete;
    PhotoPeakFitInput& operator=(const PhotoPeakFitInput&) = delete;
    void Describe(PhotoPeakFitResult& result, const PhotoPeakFitConfig& config) const;

    TH1* histogram = nullptr;
    PhotoPeakBackgroundEstimate background;

  private:
    std::unique_ptr<TH1> fSubtracted;
};

#endif

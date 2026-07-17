#ifndef __PHOTOPEAKFITCONFIG_H__
#define __PHOTOPEAKFITCONFIG_H__

#include <array>
#include <string>

#include <PhotoPeakFit/PhotoPeakFitResult.h>

enum class PhotoPeakFitMode { Auto, HighStat, LowStat };
enum class PhotoPeakBackgroundMode { None, Local, Global };
enum class PhotoPeakParameterMode { Free, Fixed, Limited };

struct PhotoPeakParameterControl {
  double value = 0.0;
  PhotoPeakParameterMode mode = PhotoPeakParameterMode::Free;
  double lower = 0.0;
  double upper = 0.0;
};

struct PhotoPeakFitConfig {
  PhotoPeakFitMode mode = PhotoPeakFitMode::Auto;
  PhotoPeakBackgroundMode background = PhotoPeakBackgroundMode::None;
  std::string rootOptions = "RQSN";
  int backgroundIterations = 20;
  std::string backgroundOptions = "BackDecreasingWindow BackOrder2 nosmoothing";
  double backgroundLow = 0.0;
  double backgroundHigh = 0.0;
  bool relativePosition = false;
  bool relativeFwhm = true;
  std::array<PhotoPeakParameterControl, kPhotoPeakNPars> global{};
  PhotoPeakParameterControl widthScale{1.0, PhotoPeakParameterMode::Free,
                                       0.1, 10.0};

  static PhotoPeakFitConfig Defaults();
  void Normalize(std::size_t peakCount);
};

#endif

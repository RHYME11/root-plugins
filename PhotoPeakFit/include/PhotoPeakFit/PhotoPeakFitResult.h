#ifndef __PHOTOPEAKFITRESULT_H__
#define __PHOTOPEAKFITRESULT_H__

#include <array>
#include <string>

enum PhotoPeakParameter {
  kPhotoPeakA = 0,
  kPhotoPeakB,
  kPhotoPeakC,
  kPhotoPeakR,
  kPhotoPeakBeta,
  kPhotoPeakStep,
  kPhotoPeakPosition,
  kPhotoPeakFwhm,
  kPhotoPeakHeight,
  kPhotoPeakNPars
};

struct PhotoPeakFitResult {
  int status = 1;
  std::string model;
  int fitBins = 0;
  int freeParameters = 0;
  int ndf = 0;
  double chi2 = 0.0;
  double reducedChi2 = 0.0;
  double area = 0.0;
  double areaError = 0.0;
  std::array<double, kPhotoPeakNPars> parameters{};
  std::array<double, kPhotoPeakNPars> errors{};
};

#endif

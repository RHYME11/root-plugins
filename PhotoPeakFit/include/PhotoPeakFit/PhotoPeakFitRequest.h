#ifndef __PHOTOPEAKFITREQUEST_H__
#define __PHOTOPEAKFITREQUEST_H__

#include <vector>

#include <PhotoPeakFit/PhotoPeakFitConfig.h>

struct PhotoPeakSeed {
  PhotoPeakParameterControl centroid;
  PhotoPeakParameterControl height;
  PhotoPeakParameterControl fwhm;
};

struct PhotoPeakFitRequest {
  double fitLow = 0.0;
  double fitHigh = 0.0;
  std::vector<PhotoPeakSeed> peaks;
  PhotoPeakFitConfig config = PhotoPeakFitConfig::Defaults();
};

#endif

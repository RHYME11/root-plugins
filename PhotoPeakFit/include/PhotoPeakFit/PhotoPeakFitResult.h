#ifndef __PHOTOPEAKFITRESULT_H__
#define __PHOTOPEAKFITRESULT_H__

#include <array>
#include <string>
#include <vector>

struct PhotoPeakFitRequest;

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
  std::string message;
  std::string backgroundMode = "None";
  bool backgroundSubtracted = false;
  int backgroundIterations = 0;
  double backgroundLow = 0.0;
  double backgroundHigh = 0.0;
  int fitBins = 0;
  int freeParameters = 0;
  int ndf = 0;
  double chi2 = 0.0;
  double reducedChi2 = 0.0;
  double area = 0.0;
  double areaError = 0.0;
  std::array<double, kPhotoPeakNPars> parameters{};
  std::array<double, kPhotoPeakNPars> errors{};
  bool relativeFwhm = false;
  double widthScale = 1.0;
  double widthScaleError = 0.0;
  struct Peak {
    double centroid = 0.0;
    double centroidError = 0.0;
    double height = 0.0;
    double heightError = 0.0;
    double fwhm = 0.0;
    double fwhmError = 0.0;
    double area = 0.0;
    double areaError = 0.0;
  };
  std::vector<Peak> peaks;
  struct Parameter {
    std::string name;
    double value = 0.0;
    double error = 0.0;
    double lower = 0.0;
    double upper = 0.0;
    bool fixed = false;
  };
  std::vector<Parameter> fitParameters;

  std::string GlobalParameterReport() const;
  std::string BackgroundReport() const;
  std::string Report(const std::string& histogram, const PhotoPeakFitRequest& request) const;
};

#endif

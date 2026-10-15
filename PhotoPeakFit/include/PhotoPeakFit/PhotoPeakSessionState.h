#ifndef __PHOTOPEAKSESSIONSTATE_H__
#define __PHOTOPEAKSESSIONSTATE_H__

#include <PhotoPeakFit/PhotoPeakFitRequest.h>

enum class PhotoPeakInteraction { Peaks, Range, Browse };

struct PhotoPeakSessionState {
  PhotoPeakFitRequest request;
  bool closed = false;
  int rebinFactor = 1;
  PhotoPeakInteraction interaction = PhotoPeakInteraction::Peaks;
  std::uint64_t selectedPeak = 0;
  std::uint64_t revision = 0;
  std::uint64_t resultRevision = 0;
  bool rangeVisible = false;
  bool hasResult = false;
  bool fitting = false;
  int fittedRebinFactor = 1;
  PhotoPeakFitRequest fittedRequest;
  PhotoPeakFitResult result;
  std::string status = "Left-click to add or select a peak; Shift-click to set the range.";
};

struct PhotoPeakInputEvent {
  int type = 0;
  int code = 0;
  int px = 0;
  double x = 0.0;
  bool inPlot = true;
};

#endif

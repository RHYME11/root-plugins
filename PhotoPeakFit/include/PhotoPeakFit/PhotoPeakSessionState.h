#ifndef __PHOTOPEAKSESSIONSTATE_H__
#define __PHOTOPEAKSESSIONSTATE_H__

#include <PhotoPeakFit/PhotoPeakFitRequest.h>

struct PhotoPeakSessionState {
  PhotoPeakFitRequest request;
  bool closed = false;
  int rebinFactor = 1;
};

struct PhotoPeakInputEvent {
  int type = 0;
  int code = 0;
  int px = 0;
  double x = 0.0;
};

#endif

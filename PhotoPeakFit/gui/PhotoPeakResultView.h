#ifndef PHOTOPEAK_RESULT_VIEW_H
#define PHOTOPEAK_RESULT_VIEW_H

#include <TGTextView.h>

class PhotoPeakResultView : public TGTextView {
public:
  PhotoPeakResultView(const TGWindow* parent, UInt_t width, UInt_t height);
  ~PhotoPeakResultView() override;

protected:
  void DrawRegion(Int_t x, Int_t y, UInt_t width, UInt_t height) override;

  Pixmap_t fBuffer = 0;
  TGDimension fBufferSize;
};

#endif

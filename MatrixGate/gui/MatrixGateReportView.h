#ifndef MATRIXGATE_REPORT_VIEW_H
#define MATRIXGATE_REPORT_VIEW_H

#include <TGTextView.h>

class MatrixGateReportView : public TGTextView {
public:
  MatrixGateReportView(const TGWindow* parent, UInt_t width, UInt_t height);
  ~MatrixGateReportView() override;

protected:
  void DrawRegion(Int_t x, Int_t y, UInt_t width, UInt_t height) override;

  Pixmap_t fBuffer = 0;
  TGDimension fBufferSize;
};

#endif

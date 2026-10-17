#include "PhotoPeakResultView.h"

#include <algorithm>
#include <memory>

#include <TVirtualX.h>

// ============== PhotoPeakResultView::PhotoPeakResultView ==============
// Purpose: Keep native text selection and scrolling with bounded rendering.
// Inputs: Parent and initial size. Outputs: Parent-owned results viewer.
PhotoPeakResultView::PhotoPeakResultView(const TGWindow* parent, UInt_t width,
  UInt_t height) : TGTextView(parent, width, height) {}

// ============== PhotoPeakResultView::~PhotoPeakResultView ==============
// Purpose: Release the private drawing surface.
// Inputs: Owned buffer. Outputs: Released native pixmap.
PhotoPeakResultView::~PhotoPeakResultView() {
  if(fBuffer)
    gVirtualX->DeletePixmap(fBuffer);
}

// ============== PhotoPeakResultView::DrawRegion ==============
// Purpose: Clip every text and selection draw to the visible viewport.
// Inputs: Native repaint request. Outputs: Bounded text image on the canvas.
void PhotoPeakResultView::DrawRegion(Int_t, Int_t, UInt_t, UInt_t) {
  const auto width = fCanvas->GetWidth();
  const auto height = fCanvas->GetHeight();
  if(!width || !height)
    return;
  if(!fBuffer || fBufferSize.fWidth != width || fBufferSize.fHeight != height) {
    if(fBuffer)
      gVirtualX->DeletePixmap(fBuffer);
    fBuffer = gVirtualX->CreatePixmap(fCanvas->GetId(), width, height);
    fBufferSize = TGDimension(width, height);
  }
  if(!fBuffer)
    return;

  // Cocoa redirects TGViewFrame text into its parent without a viewport clip.
  // A viewport-sized pixmap bounds glyphs and selection before that redirection.
  gVirtualX->FillRectangle(fBuffer, fWhiteGC(), 0, 0, width, height);
  const Long_t rowHeight = std::max(1, fMaxAscent + fMaxDescent);
  for(Long_t row = std::max(0L, fVisible.fY / rowHeight); row < fText->RowCount(); ++row) {
    const Long_t top = row * rowHeight - fVisible.fY;
    if(top >= static_cast<Long_t>(height))
      break;
    const Long_t start = ToObjXCoord(fVisible.fX, row);
    const Long_t end = std::min(fText->GetLineLength(row),
      ToObjXCoord(fVisible.fX + width, row) + 1);
    const Long_t length = end - start;
    if(length <= 0)
      continue;
    std::unique_ptr<char[]> text(fText->GetLine({start, row}, length));
    if(!text)
      continue;
    for(Long_t column = 0; column < length; ++column) {
      if(text[column] == '\t' || text[column] == 16)
        text[column] = ' ';
    }
    const Int_t left = start ? ToScrXCoord(start, row) : -fVisible.fX;
    const Int_t baseline = top + fMaxAscent;
    Long_t markedStart = start;
    Long_t markedEnd = start;
    if(fIsMarked && row >= fMarkedStart.fY && row <= fMarkedEnd.fY) {
      markedStart = row == fMarkedStart.fY ? std::clamp(fMarkedStart.fX, start, end) : start;
      markedEnd = row == fMarkedEnd.fY ? std::clamp(fMarkedEnd.fX, start, end) : end;
    }
    if(markedEnd <= markedStart) {
      gVirtualX->DrawString(fBuffer, fNormGC(), left, baseline, text.get(), length);
      continue;
    }
    const Int_t prefix = markedStart - start;
    const Int_t selected = markedEnd - markedStart;
    const Int_t selectionLeft = left + gVirtualX->TextWidth(fFont, text.get(), prefix);
    const Int_t selectionWidth = gVirtualX->TextWidth(fFont, text.get() + prefix, selected);
    if(prefix)
      gVirtualX->DrawString(fBuffer, fNormGC(), left, baseline, text.get(), prefix);
    gVirtualX->FillRectangle(fBuffer, fSelbackGC(), selectionLeft, top, selectionWidth, rowHeight);
    gVirtualX->DrawString(fBuffer, fSelGC(), selectionLeft, baseline, text.get() + prefix, selected);
    if(prefix + selected < length)
      gVirtualX->DrawString(fBuffer, fNormGC(), selectionLeft + selectionWidth, baseline,
        text.get() + prefix + selected, length - prefix - selected);
  }
  gVirtualX->CopyArea(fBuffer, fCanvas->GetId(), fNormGC(), 0, 0, width, height, 0, 0);
}

#include "PhotoPeakResultView.h"

#include <algorithm>
#include <memory>

#include <Buttons.h>
#include <KeySymbols.h>
#include <TGScrollBar.h>
#include <TVirtualX.h>

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

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

// ============== PhotoPeakResultView::LoadBuffer ==============
// Purpose: Keep selection and scrolling when only the surrounding controls refresh.
// Inputs: Report text. Outputs: Replaced content only when the report changes.
Bool_t PhotoPeakResultView::LoadBuffer(const char* text) {
  if(!text || !*text) {
    Clear();
    return kTRUE;
  }
  if(fReport == text)
    return kTRUE;
  if(!TGTextView::LoadBuffer(text))
    return kFALSE;
  fReport = text;
  SetHsbPosition(0);
  SetVsbPosition(0);
  return kTRUE;
}

// ============== PhotoPeakResultView::Clear ==============
// Purpose: Clear the rendered report and its refresh cache together.
// Inputs: ROOT clear option. Outputs: Empty viewer.
void PhotoPeakResultView::Clear(Option_t* option) {
  fReport.clear();
  TGTextView::Clear(option);
}

// ============== PhotoPeakResultView::Copy ==============
// Purpose: Export the selected text to the system clipboard for other applications.
// Inputs: Current selection. Outputs: Clipboard text and copy status.
Bool_t PhotoPeakResultView::Copy() {
  if(!fIsMarked || (fMarkedStart.fX == fMarkedEnd.fX && fMarkedStart.fY == fMarkedEnd.fY) ||
     !TGTextView::Copy())
    return kFALSE;
  if(fSelecting)
    return kTRUE;
#ifdef __APPLE__
  // ROOT's primary-selection buffer drops a newline when the selection ends at column zero.
  std::string text;
  for(Long_t row = fMarkedStart.fY; row <= fMarkedEnd.fY; ++row) {
    const Long_t first = row == fMarkedStart.fY ? fMarkedStart.fX : 0;
    const Long_t last = row == fMarkedEnd.fY ? fMarkedEnd.fX : fText->GetLineLength(row);
    if(last > first) {
      std::unique_ptr<char[]> line(fText->GetLine({first, row}, last - first));
      if(line) text.append(line.get(), last - first);
    }
    if(row < fMarkedEnd.fY) text += '\n';
  }
  text.erase(std::remove(text.begin(), text.end(), '\x10'), text.end());
  CFDataRef data = CFDataCreate(kCFAllocatorDefault,
    reinterpret_cast<const UInt8*>(text.data()), static_cast<CFIndex>(text.size()));
  if(!data)
    return kFALSE;
  PasteboardRef clipboard = nullptr;
  OSStatus status = PasteboardCreate(kPasteboardClipboard, &clipboard);
  if(status == noErr) {
    status = PasteboardClear(clipboard);
    if(status == noErr)
      status = PasteboardPutItemFlavor(clipboard, this,
        CFSTR("public.utf8-plain-text"), data, kPasteboardFlavorNoFlags);
    CFRelease(clipboard);
  }
  CFRelease(data);
  return status == noErr;
#else
  Atom_t clipboard = gVirtualX->InternAtom("CLIPBOARD", kFALSE);
  return gVirtualX->SetSelectionOwner(GetId(), clipboard);
#endif
}

// ============== PhotoPeakResultView::SelectAll ==============
// Purpose: Select the report without replacing the system clipboard before an explicit copy.
// Inputs: Loaded report. Outputs: Native full-text selection.
Bool_t PhotoPeakResultView::SelectAll() {
  const bool selecting = fSelecting;
  fSelecting = true;
  const auto selected = TGTextView::SelectAll();
  fSelecting = selecting;
  return selected;
}

// ============== PhotoPeakResultView::CopyAll ==============
// Purpose: Copy the complete report, including text outside the visible viewport.
// Inputs: Loaded report. Outputs: Full selection and system clipboard text.
Bool_t PhotoPeakResultView::CopyAll() {
  return SelectAll() && Copy();
}

// ============== PhotoPeakResultView::HandleButton ==============
// Purpose: Focus the report for clipboard shortcuts while retaining native text selection.
// Inputs: Mouse button event. Outputs: Focused viewer and native selection handling.
Bool_t PhotoPeakResultView::HandleButton(Event_t* event) {
  if(event && event->fWindow == fCanvas->GetId() &&
     event->fType == kButtonPress && event->fCode == kButton1)
    gVirtualX->SetInputFocus(fCanvas->GetId());
  const bool selecting = fSelecting;
  fSelecting = true;
  const auto handled = TGTextView::HandleButton(event);
  fSelecting = selecting;
  return handled;
}

// ============== PhotoPeakResultView::HandleKey ==============
// Purpose: Support standard copy and select-all shortcuts inside the report.
// Inputs: Focused key event. Outputs: Clipboard action or native key handling.
Bool_t PhotoPeakResultView::HandleKey(Event_t* event) {
  if(!event || event->fType != kGKeyPress)
    return kFALSE;
  Mask_t modifiers = kKeyControlMask;
#ifdef __APPLE__
  modifiers |= kKeyMod2Mask; // ROOT maps the macOS Command key to Mod2.
#endif
  if(event->fState & modifiers) {
    UInt_t key = 0;
    char text[8] = {};
    gVirtualX->LookupString(event, text, sizeof(text), key);
    if(key == kKey_c || key == kKey_C)
      return Copy();
    if(key == kKey_a || key == kKey_A)
      return SelectAll();
  }
  return TGTextView::HandleKey(event);
}

// ============== PhotoPeakResultView::ProcessMessage ==============
// Purpose: Keep delayed scrollbar messages within the current report's visible bounds.
// Inputs: ROOT scroll message and position. Outputs: Valid viewport or native message handling.
Bool_t PhotoPeakResultView::ProcessMessage(Longptr_t message, Longptr_t position, Longptr_t data) {
  const int kind = GET_MSG(message);
  if((kind == kC_HSCROLL || kind == kC_VSCROLL) &&
     (GET_SUBMSG(message) == kSB_SLIDERPOS || GET_SUBMSG(message) == kSB_SLIDERTRACK)) {
    TGScrollBar* bar = kind == kC_HSCROLL ? static_cast<TGScrollBar*>(fHsb) : fVsb;
    const Longptr_t maximum = bar && bar->IsMapped() ? std::max(0, bar->GetRange() - bar->GetPageSize()) : 0;
    position = std::clamp<Longptr_t>(position, 0, maximum);
  }
  return TGTextView::ProcessMessage(message, position, data);
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

#ifndef PHOTOPEAK_RESULT_VIEW_H
#define PHOTOPEAK_RESULT_VIEW_H

#include <TGTextView.h>
#include <string>

class PhotoPeakResultView : public TGTextView {
public:
  PhotoPeakResultView(const TGWindow* parent, UInt_t width, UInt_t height);
  ~PhotoPeakResultView() override;
  Bool_t LoadBuffer(const char* text) override;
  Bool_t LoadBuffer(const char* text, bool highlightHeading);
  void Clear(Option_t* option = "") override;
  Bool_t Copy() override;
  Bool_t SelectAll() override;
  Bool_t CopyAll();
  Bool_t HandleButton(Event_t* event) override;
  Bool_t HandleKey(Event_t* event) override;
  Bool_t ProcessMessage(Longptr_t message, Longptr_t position, Longptr_t data) override;

protected:
  void DrawRegion(Int_t x, Int_t y, UInt_t width, UInt_t height) override;

  Pixmap_t fBuffer = 0;
  TGDimension fBufferSize;

private:
  TGGC fHeadingGC;
  std::string fReport;
  bool fHighlightHeading = false;
  bool fSelecting = false;
};

#endif

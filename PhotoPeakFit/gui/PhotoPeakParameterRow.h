#ifndef __PHOTOPEAKPARAMETERROW_H__
#define __PHOTOPEAKPARAMETERROW_H__

#include <PhotoPeakFit/PhotoPeakFitConfig.h>

class TGCompositeFrame;
class TGHorizontalFrame;
class TGNumberEntry;
class TGCheckButton;
class TGWindow;

class PhotoPeakParameterRow {
  public:
    PhotoPeakParameterRow(TGCompositeFrame* parent, const char* label,
                          int identifier, TGWindow* receiver);
    void Set(const PhotoPeakParameterControl& control);
    PhotoPeakParameterControl Get();
    void HandleToggle(int identifier);
    void SetEnabled(bool enabled);

  private:
    void Normalize();

    TGHorizontalFrame* fFrame;
    TGNumberEntry* fValue;
    TGCheckButton* fFixed;
    TGCheckButton* fLimited;
    TGNumberEntry* fLower;
    TGNumberEntry* fUpper;
    int fIdentifier;
};

#endif

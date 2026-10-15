#ifndef PHOTOPEAK_PARAMETER_ROW_H
#define PHOTOPEAK_PARAMETER_ROW_H

#include <string>
#include <PhotoPeakFit/PhotoPeakFitConfig.h>

class PhotoPeakEntry;
class TGCompositeFrame;
class TGVerticalFrame;
class TGHorizontalFrame;
class TGComboBox;
class TGWindow;

class PhotoPeakParameterRow {
public:
  PhotoPeakParameterRow(TGCompositeFrame* parent, const char* label,
                        int identifier, TGWindow* receiver);
  void Set(const PhotoPeakParameterControl& control);
  bool Get(PhotoPeakParameterControl& control, std::string& error) const;
  void SetEnabled(bool enabled);
  void Update();

private:
  std::string fLabel;
  TGVerticalFrame* fFrame;
  TGHorizontalFrame* fLimits;
  PhotoPeakEntry* fValue;
  PhotoPeakEntry* fLower;
  PhotoPeakEntry* fUpper;
  TGComboBox* fMode;
  bool fEnabled = true;
};

#endif

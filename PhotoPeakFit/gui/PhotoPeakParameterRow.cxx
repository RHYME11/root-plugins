#include "PhotoPeakParameterRow.h"

#include <algorithm>

#include <TGButton.h>
#include <TGLabel.h>
#include <TGNumberEntry.h>

PhotoPeakParameterRow::PhotoPeakParameterRow(TGCompositeFrame* parent,
                                             const char* label,
                                             int identifier,
                                             TGWindow* receiver)
  : fIdentifier(identifier) {
  fFrame = new TGHorizontalFrame(parent);
  fFrame->SetCleanup(kDeepCleanup);
  auto* rowLabel = new TGLabel(fFrame, label);
  rowLabel->SetTextJustify(kTextRight);
  rowLabel->Resize(84, rowLabel->GetDefaultHeight());
  fFrame->AddFrame(rowLabel,
                   new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 2, 6, 2, 2));
  fValue = new TGNumberEntry(fFrame, 0.0, 9, identifier);
  fValue->Resize(128, fValue->GetDefaultHeight());
  fFrame->AddFrame(fValue, new TGLayoutHints(kLHintsLeft, 2, 4, 2, 2));
  fFixed = new TGCheckButton(fFrame, "fix", identifier + 1);
  fLimited = new TGCheckButton(fFrame, "limits", identifier + 2);
  fFixed->Resize(58, fFixed->GetDefaultHeight());
  fLimited->Resize(78, fLimited->GetDefaultHeight());
  fFixed->Associate(receiver);
  fLimited->Associate(receiver);
  fFrame->AddFrame(fFixed, new TGLayoutHints(kLHintsLeft, 2, 2, 2, 2));
  fFrame->AddFrame(fLimited, new TGLayoutHints(kLHintsLeft, 2, 4, 2, 2));
  fLower = new TGNumberEntry(fFrame, 0.0, 9, identifier + 3);
  fUpper = new TGNumberEntry(fFrame, 0.0, 9, identifier + 4);
  fLower->Resize(120, fLower->GetDefaultHeight());
  fUpper->Resize(120, fUpper->GetDefaultHeight());
  fFrame->AddFrame(fLower, new TGLayoutHints(kLHintsLeft, 2, 2, 2, 2));
  fFrame->AddFrame(fUpper, new TGLayoutHints(kLHintsLeft, 2, 2, 2, 2));
  parent->AddFrame(fFrame, new TGLayoutHints(kLHintsExpandX));
}

void PhotoPeakParameterRow::Set(const PhotoPeakParameterControl& control) {
  fValue->SetNumber(control.value);
  fFixed->SetState(control.mode == PhotoPeakParameterMode::Fixed ?
                   kButtonDown : kButtonUp, false);
  fLimited->SetState(control.mode == PhotoPeakParameterMode::Limited ?
                     kButtonDown : kButtonUp, false);
  fLower->SetNumber(control.lower);
  fUpper->SetNumber(control.upper);
  Normalize();
}

PhotoPeakParameterControl PhotoPeakParameterRow::Get() {
  Normalize();
  PhotoPeakParameterControl control;
  control.value = fValue->GetNumber();
  control.lower = fLower->GetNumber();
  control.upper = fUpper->GetNumber();
  if(control.lower > control.upper)
    std::swap(control.lower, control.upper);
  if(fFixed->IsOn())
    control.mode = PhotoPeakParameterMode::Fixed;
  else if(fLimited->IsOn())
    control.mode = PhotoPeakParameterMode::Limited;
  return control;
}

void PhotoPeakParameterRow::Normalize() {
  if(fFixed->IsOn() && fLimited->IsOn())
    fLimited->SetState(kButtonUp, false);
  const bool limits = fLimited->IsOn() && !fFixed->IsOn();
  fLower->SetState(limits);
  fUpper->SetState(limits);
}

void PhotoPeakParameterRow::HandleToggle(int identifier) {
  if(identifier == fIdentifier + 1 && fFixed->IsOn())
    fLimited->SetState(kButtonUp, false);
  if(identifier == fIdentifier + 2 && fLimited->IsOn())
    fFixed->SetState(kButtonUp, false);
  Normalize();
}

void PhotoPeakParameterRow::SetEnabled(bool enabled) {
  fValue->SetState(enabled);
  fFixed->SetEnabled(enabled);
  fLimited->SetEnabled(enabled);
  if(!enabled) {
    fLower->SetState(false);
    fUpper->SetState(false);
  } else {
    Normalize();
  }
}

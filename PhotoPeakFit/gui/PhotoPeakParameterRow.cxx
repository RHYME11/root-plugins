#include "PhotoPeakParameterRow.h"
#include "PhotoPeakEntry.h"

#include <TGComboBox.h>
#include <TGLabel.h>

// ============== PhotoPeakParameterRow::PhotoPeakParameterRow ==============
// Purpose: Build a value/mode row with expandable limits.
// Inputs: Parent, label, ID range, receiver. Outputs: Parent-owned widgets.
PhotoPeakParameterRow::PhotoPeakParameterRow(TGCompositeFrame* parent,
  const char* label, int identifier, TGWindow* receiver) : fLabel(label) {
  fFrame = new TGVerticalFrame(parent);
  auto* main = new TGHorizontalFrame(fFrame);
  auto* name = new TGLabel(main, label);
  name->SetTextJustify(kTextLeft);
  name->ChangeOptions(name->GetOptions() | kFixedWidth);
  name->Resize(70, name->GetDefaultHeight());
  main->AddFrame(name, new TGLayoutHints(kLHintsLeft | kLHintsCenterY, 2, 6, 3, 3));
  fValue = new PhotoPeakEntry(main, identifier, receiver);
  main->AddFrame(fValue, new TGLayoutHints(kLHintsExpandX, 2, 6, 3, 3));
  fMode = new TGComboBox(main, identifier + 1);
  fMode->AddEntry("Free", 0);
  fMode->AddEntry("Fixed", 1);
  fMode->AddEntry("Limited", 2);
  fMode->Resize(90, 24);
  fMode->Associate(receiver);
  main->AddFrame(fMode, new TGLayoutHints(kLHintsRight, 2, 2, 3, 3));
  fFrame->AddFrame(main, new TGLayoutHints(kLHintsExpandX));
  fLimits = new TGHorizontalFrame(fFrame);
  fLimits->AddFrame(new TGLabel(fLimits, "Lower"),
    new TGLayoutHints(kLHintsCenterY, 12, 4, 2, 2));
  fLower = new PhotoPeakEntry(fLimits, identifier + 2, receiver);
  fLimits->AddFrame(fLower, new TGLayoutHints(kLHintsExpandX, 2, 6, 2, 2));
  fLimits->AddFrame(new TGLabel(fLimits, "Upper"),
    new TGLayoutHints(kLHintsCenterY, 2, 4, 2, 2));
  fUpper = new PhotoPeakEntry(fLimits, identifier + 3, receiver);
  fLimits->AddFrame(fUpper, new TGLayoutHints(kLHintsExpandX, 2, 2, 2, 2));
  fFrame->AddFrame(fLimits, new TGLayoutHints(kLHintsExpandX));
  parent->AddFrame(fFrame, new TGLayoutHints(kLHintsExpandX));
  Set(PhotoPeakParameterControl{});
}

// ============== PhotoPeakParameterRow::Set ==============
// Purpose: Show stored settings. Inputs: Control. Outputs: Value, mode and limits.
void PhotoPeakParameterRow::Set(const PhotoPeakParameterControl& control) {
  fValue->SetNumber(control.value);
  fLower->SetNumber(control.lower);
  fUpper->SetNumber(control.upper);
  fMode->Select(static_cast<int>(control.mode), false);
  Update();
}

// ============== PhotoPeakParameterRow::Get ==============
// Purpose: Read edits without silent correction. Inputs: Widgets. Outputs: Control or error.
bool PhotoPeakParameterRow::Get(PhotoPeakParameterControl& control,
  std::string& error) const {
  control.mode = static_cast<PhotoPeakParameterMode>(fMode->GetSelected());
  if(!fValue->Read(control.value, fLabel.c_str(), error))
    return false;
  if(control.mode != PhotoPeakParameterMode::Limited)
    return true;
  return fLower->Read(control.lower, (fLabel + " lower").c_str(), error) &&
    fUpper->Read(control.upper, (fLabel + " upper").c_str(), error);
}

// ============== PhotoPeakParameterRow::SetEnabled ==============
// Purpose: Retain inactive settings. Inputs: Enabled flag. Outputs: Widget states.
void PhotoPeakParameterRow::SetEnabled(bool enabled) {
  fEnabled = enabled;
  fValue->SetEnabled(enabled);
  fMode->SetEnabled(enabled);
  Update();
}

// ============== PhotoPeakParameterRow::Update ==============
// Purpose: Expose Limited bounds. Inputs: Mode. Outputs: Visible limits and layout.
void PhotoPeakParameterRow::Update() {
  const bool limited = fMode->GetSelected() == 2;
  if(limited)
    fFrame->ShowFrame(fLimits);
  else
    fFrame->HideFrame(fLimits);
  fLower->SetEnabled(fEnabled && limited);
  fUpper->SetEnabled(fEnabled && limited);
  fFrame->Layout();
}

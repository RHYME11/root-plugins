#ifndef PHOTOPEAK_ENTRY_H
#define PHOTOPEAK_ENTRY_H

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <string>

#include <TGTextEntry.h>
#include <TString.h>

class PhotoPeakEntry : public TGTextEntry {
public:
  // ============== PhotoPeakEntry ==============
  // Purpose: Keep numeric drafts until validation. Inputs: Parent, ID, receiver. Outputs: Entry.
  PhotoPeakEntry(const TGWindow* parent, int id, TGWindow* receiver)
    : TGTextEntry(parent, "", id) {
    Associate(receiver);
    SetDefaultSize(100, 24);
  }

  // ============== SetNumber ==============
  // Purpose: Render a committed number. Inputs: Value. Outputs: Text without edit messages.
  void SetNumber(double value) {
    SetText(TString::Format("%.12g", value), false);
  }

  // ============== Read ==============
  // Purpose: Reject incomplete or non-finite text. Inputs: Label. Outputs: Number or error.
  bool Read(double& value, const char* label, std::string& error) const {
    const char* start = GetText();
    char* end = nullptr;
    errno = 0;
    value = std::strtod(start, &end);
    const bool empty = start == end;
    if(end) {
      while(std::isspace(static_cast<unsigned char>(*end)))
        ++end;
    }
    if(empty || !end || *end || errno == ERANGE || !std::isfinite(value)) {
      error = std::string(label) + ": enter a finite number.";
      return false;
    }
    return true;
  }

  // ============== HandleFocusChange ==============
  // Purpose: Commit on focus loss. Inputs: Native event. Outputs: Validation message.
  Bool_t HandleFocusChange(Event_t* event) override {
    const Bool_t handled = TGTextEntry::HandleFocusChange(event);
    if(event->fType == kFocusOut)
      SendMessage(fMsgWindow, MK_MSG(kC_TEXTENTRY, kTE_ENTER), WidgetId(), 0);
    return handled;
  }
};

#endif

#ifndef PHOTOPEAK_ROOT_CONTROLLER_H
#define PHOTOPEAK_ROOT_CONTROLLER_H

#include <memory>
#include <TObject.h>
#include <GuiTypes.h>

class TH1;
class PhotoPeakSession;
class TVirtualPad;

class PhotoPeakRootController : public TObject {
public:
  static PhotoPeakRootController& Instance();
  ~PhotoPeakRootController() override;
  bool Start(TH1* histogram, TVirtualPad* pad);
  PhotoPeakSession* ActiveSession() const;
  void ParkEditor(PhotoPeakSession* session);
  bool SelectLayer(int index);
  bool CopyPrevious();
  bool Clean(TVirtualPad* pad);
  bool RefreshSnapshot(TH1* target, const TH1* snapshot, TVirtualPad* pad);
  bool InvalidateSource(TVirtualPad* pad, const char* message);
  bool Stop(TVirtualPad* pad);
  void StopAll();
  const char* LastError() const;
  void OnEvent(Int_t event, Int_t px, Int_t py, TObject* selected);
  void OnGuiEvent(Event_t* event, Window_t window);
  void OnCanvasClosed();
  void RecursiveRemove(TObject* object) override;
  void Poll();

  ClassDefOverride(PhotoPeakRootController, 0)

private:
  void Activate(PhotoPeakSession* session);
  PhotoPeakRootController();
  class Impl;
  std::unique_ptr<Impl> fImpl; //! Transient runtime state.
};

#endif

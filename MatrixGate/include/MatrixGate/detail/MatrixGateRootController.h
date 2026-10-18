#ifndef MATRIXGATE_ROOT_CONTROLLER_H
#define MATRIXGATE_ROOT_CONTROLLER_H

#include <memory>
#include <TObject.h>
#include <MatrixGate/MatrixGateConfig.h>

class TH2;

namespace MatrixGate::Detail {

class Session;

class RootController : public TObject {
public:
  static RootController& Instance();
  ~RootController() override;
  bool Start(TH2* matrix, const Config& config);
  bool Update(TH2* matrix);
  bool Stop(TH2* matrix);
  void StopAll();
  const char* LastError() const;
  Session* Find(TH2* matrix);
  void Poll();
  void OnCanvasEvent(Int_t event, Int_t px, Int_t py, TObject* selected);
  void RecursiveRemove(TObject* object) override;

  ClassDefOverride(RootController, 0)

private:
  RootController();
  class Impl;
  std::unique_ptr<Impl> fImpl; //! Transient sessions and synchronous timer.
};

} // namespace MatrixGate::Detail

#endif

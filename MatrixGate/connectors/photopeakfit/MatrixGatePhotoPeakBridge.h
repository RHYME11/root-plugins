#ifndef MATRIXGATE_PHOTOPEAK_BRIDGE_H
#define MATRIXGATE_PHOTOPEAK_BRIDGE_H

#include <memory>
#include <string>
#include <vector>
#include <TObject.h>

class TH1D;
class TCanvas;

namespace MatrixGate::Detail {

class PhotoPeakBridge : public TObject {
public:
  PhotoPeakBridge();
  ~PhotoPeakBridge() override;
  bool Open(const std::string& key, const std::string& title, bool total, const TH1D& spectrum, std::string& error);
  void InvalidateGated(const std::string& reason);
  void InvalidateAll(const std::string& reason);
  void CheckTotal(const TH1D& spectrum);
  void Close(bool gatedOnly = false);
  void RecursiveRemove(TObject* object) override;
private:
  struct Context;
  struct Api;
  bool Load(std::string& error);
  void MarkStale(Context& context, const std::string& reason);
  std::unique_ptr<Api> fApi;
  std::vector<std::unique_ptr<Context>> fFits;
};

} // namespace MatrixGate::Detail
#endif

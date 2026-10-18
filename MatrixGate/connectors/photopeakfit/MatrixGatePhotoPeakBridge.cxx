#include "MatrixGatePhotoPeakBridge.h"

#include <algorithm>
#include <filesystem>
#include <TCanvas.h>
#include <TDirectory.h>
#include <TH1D.h>
#include <TList.h>
#include <TPaveText.h>
#include <TROOT.h>
#include <TSystem.h>
#if !defined(_WIN32)
#include <dlfcn.h>
#endif

namespace MatrixGate::Detail {
namespace {

// ============== BridgeAnchor ==============
// Purpose: Locate this library for optional sibling loading. Inputs: None. Outputs: In-library symbol address.
void BridgeAnchor() {}

// ============== Alive ==============
// Purpose: Test native canvas registration before use. Inputs: Borrowed canvas. Outputs: Liveness.
bool Alive(TCanvas* canvas) { return canvas && gROOT && gROOT->GetListOfCanvases()->FindObject(canvas); }

// ============== CloneSnapshot ==============
// Purpose: Own a detached histogram with no fit artifacts. Inputs: Spectrum/name. Outputs: Snapshot copy.
std::unique_ptr<TH1D> CloneSnapshot(const TH1D& source, const std::string& name) {
  TDirectory::TContext context(nullptr);
  auto snapshot = std::unique_ptr<TH1D>(static_cast<TH1D*>(source.Clone(name.c_str())));
  snapshot->SetDirectory(nullptr); snapshot->SetStats(false); snapshot->ResetBit(TObject::kCanDelete);
  snapshot->GetListOfFunctions()->Delete();
  return snapshot;
}

// ============== Same ==============
// Purpose: Compare immutable data independently of fitter rebin/view changes. Inputs: Spectra. Outputs: Equality.
bool Same(const TH1D& a, const TH1D& b) {
  if(a.GetNbinsX() != b.GetNbinsX()) return false;
  for(int bin = 0; bin <= a.GetNbinsX() + 1; ++bin)
    if(a.GetBinContent(bin) != b.GetBinContent(bin) || a.GetBinError(bin) != b.GetBinError(bin) ||
      a.GetXaxis()->GetBinLowEdge(bin) != b.GetXaxis()->GetBinLowEdge(bin)) return false;
  return true;
}

} // namespace

struct PhotoPeakBridge::Api {
  int (*version)() = nullptr;
  bool (*start)(TH1*, TVirtualPad*) = nullptr;
  bool (*stop)(TVirtualPad*) = nullptr;
  bool (*refresh)(TH1*, const TH1*, TVirtualPad*) = nullptr;
  bool (*invalidate)(TVirtualPad*, const char*) = nullptr;
  const char* (*error)() = nullptr;
};

struct PhotoPeakBridge::Context {
  std::string key, title;
  bool total = false, stale = false;
  TCanvas* canvas = nullptr;
  std::unique_ptr<TH1D> spectrum, baseline;
  std::unique_ptr<TPaveText> stamp;
};

// ============== PhotoPeakBridge ==============
// Purpose: Observe deletion of connector-owned native windows. Inputs: None. Outputs: Inactive optional bridge.
PhotoPeakBridge::PhotoPeakBridge() { gROOT->GetListOfCleanups()->Add(this); }

// ============== ~PhotoPeakBridge ==============
// Purpose: Stop owned fits before releasing snapshots. Inputs: None. Outputs: Released native resources.
PhotoPeakBridge::~PhotoPeakBridge() {
  Close();
  if(gROOT && gROOT->GetListOfCleanups()) gROOT->GetListOfCleanups()->Remove(this);
}

// ============== Load ==============
// Purpose: Resolve versioned optional C symbols without linking the gating core to PhotoPeakFit.
// Inputs: Error output. Outputs: Lazy-loaded native fitter or actionable diagnostic.
bool PhotoPeakBridge::Load(std::string& error) {
  if(fApi) return true;
  auto version = reinterpret_cast<int(*)()>(gSystem->DynFindSymbol(nullptr, "PhotoPeakFitBridgeVersion"));
  if(!version) {
#if !defined(_WIN32)
    Dl_info location{};
    if(dladdr(reinterpret_cast<void*>(&BridgeAnchor), &location) && location.dli_fname) {
      const auto directory = std::filesystem::path(location.dli_fname).parent_path();
      for(const char* filename : {"libPhotoPeakFit.dylib", "libPhotoPeakFit.so", "PhotoPeakFit.dll"}) {
        const auto library = directory / filename;
        if(std::filesystem::exists(library)) { gSystem->Load(library.string().c_str()); break; }
      }
    }
#endif
    version = reinterpret_cast<int(*)()>(gSystem->DynFindSymbol(nullptr, "PhotoPeakFitBridgeVersion"));
    if(!version) {
      std::unique_ptr<char[]> path(gSystem->DynamicPathName("libPhotoPeakFit", true));
      if(path) gSystem->Load(path.get());
      version = reinterpret_cast<int(*)()>(gSystem->DynFindSymbol(nullptr, "PhotoPeakFitBridgeVersion"));
    }
  }
  if(!version || version() != 1) {
    error = "PhotoPeakFit snapshot bridge is unavailable. Build PhotoPeakFit beside MatrixGate, then restart ROOT (or load its matching library).";
    return false;
  }
  auto api = std::make_unique<Api>(); api->version = version;
  api->start = reinterpret_cast<decltype(api->start)>(gSystem->DynFindSymbol(nullptr, "PhotoPeakFitBridgeStart"));
  api->stop = reinterpret_cast<decltype(api->stop)>(gSystem->DynFindSymbol(nullptr, "PhotoPeakFitBridgeStop"));
  api->refresh = reinterpret_cast<decltype(api->refresh)>(gSystem->DynFindSymbol(nullptr, "PhotoPeakFitBridgeRefresh"));
  api->invalidate = reinterpret_cast<decltype(api->invalidate)>(gSystem->DynFindSymbol(nullptr, "PhotoPeakFitBridgeInvalidate"));
  api->error = reinterpret_cast<decltype(api->error)>(gSystem->DynFindSymbol(nullptr, "PhotoPeakFitBridgeLastError"));
  if(!api->start || !api->stop || !api->refresh || !api->invalidate || !api->error) {
    error = "PhotoPeakFit bridge symbols are incomplete; restart ROOT with matching rebuilt libraries."; return false;
  }
  fApi = std::move(api); return true;
}

// ============== Open ==============
// Purpose: Open or explicitly refresh one independent fitting snapshot. Inputs: Stable target/title/data. Outputs: Fit window/status.
bool PhotoPeakBridge::Open(const std::string& key, const std::string& title, bool total, const TH1D& source, std::string& error) {
  if(!Load(error)) return false;
  TVirtualPad::TContext context;
  auto found = std::find_if(fFits.begin(), fFits.end(), [&](const auto& fit) { return fit->key == key; });
  if(found == fFits.end()) {
    auto fit = std::make_unique<Context>(); fit->key = key; fit->title = title; fit->total = total;
    fFits.push_back(std::move(fit)); found = std::prev(fFits.end());
  }
  auto& fit = **found;
  if(Alive(fit.canvas) && (!fit.spectrum || !fit.canvas->GetListOfPrimitives()->FindObject(fit.spectrum.get()))) {
    fApi->stop(fit.canvas);
    auto* old = fit.canvas; fit.canvas = nullptr;
    context.PadDeleted(old);
    old->Clear(); delete old;
  }
  if(!Alive(fit.canvas)) {
    if(fit.canvas) { auto* old = fit.canvas; fit.canvas = nullptr; context.PadDeleted(old); delete old; }
    fit.stamp.reset();
    static unsigned long serial = 0;
    const auto name = std::string("MatrixGateFit_") + std::to_string(++serial);
    fit.spectrum = CloneSnapshot(source, name + "_spectrum");
    fit.canvas = new TCanvas(name.c_str(), title.c_str(), 900, 600);
    fit.canvas->SetBit(TObject::kMustCleanup); fit.spectrum->Draw("hist"); fit.canvas->Update();
    if(!fApi->start(fit.spectrum.get(), fit.canvas)) { error = fApi->error(); return false; }
  } else if(fit.stale || !fit.baseline || !Same(*fit.baseline, source)) {
    if(!fApi->refresh(fit.spectrum.get(), &source, fit.canvas)) { error = fApi->error(); return false; }
  } else if(!fApi->start(fit.spectrum.get(), fit.canvas)) { error = fApi->error(); return false; }
  if(fit.stamp) { fit.canvas->GetListOfPrimitives()->Remove(fit.stamp.get()); fit.stamp.reset(); }
  fit.baseline = CloneSnapshot(source, std::string(fit.spectrum->GetName()) + "_baseline");
  fit.title = title; fit.stale = false; fit.canvas->SetTitle(title.c_str());
  fit.canvas->Modified(); fit.canvas->Update();
  return true;
}

// ============== MarkStale ==============
// Purpose: Preserve old snapshot contents while marking its fit obsolete. Inputs: Context/reason. Outputs: Visible stale state.
void PhotoPeakBridge::MarkStale(Context& fit, const std::string& reason) {
  if(fit.stale || !Alive(fit.canvas)) return;
  fit.stale = true;
  fApi->invalidate(fit.canvas, reason.c_str());
  fit.canvas->SetTitle(("OUT OF DATE | " + fit.title).c_str());
  TVirtualPad::TContext context(fit.canvas);
  fit.stamp = std::make_unique<TPaveText>(0.12, 0.87, 0.88, 0.96, "NDC");
  fit.stamp->SetFillStyle(0); fit.stamp->SetBorderSize(0); fit.stamp->SetTextColor(kRed + 1);
  fit.stamp->SetTextSize(0.03); fit.stamp->SetBit(TObject::kCannotPick);
  fit.stamp->AddText("SOURCE OUT OF DATE - reopen PhotoPeakFit from MatrixGate"); fit.stamp->Draw();
  fit.canvas->Modified(); fit.canvas->Update();
}

// ============== InvalidateGated ==============
// Purpose: Invalidate only gate-dependent fit snapshots. Inputs: Reason. Outputs: Historical gated fits.
void PhotoPeakBridge::InvalidateGated(const std::string& reason) {
  for(auto& fit : fFits) if(!fit->total) MarkStale(*fit, reason);
}

// ============== InvalidateAll ==============
// Purpose: Invalidate every snapshot when its source projection changes. Inputs: Reason. Outputs: Historical fits.
void PhotoPeakBridge::InvalidateAll(const std::string& reason) { for(auto& fit : fFits) MarkStale(*fit, reason); }

// ============== CheckTotal ==============
// Purpose: Keep total fits current across gate edits but detect changed counts/errors/binning. Inputs: Total display. Outputs: Validity.
void PhotoPeakBridge::CheckTotal(const TH1D& spectrum) {
  for(auto& fit : fFits) if(fit->total && fit->baseline && !Same(*fit->baseline, spectrum))
    MarkStale(*fit, "The total projection or its display binning changed.");
}

// ============== Close ==============
// Purpose: Purge owned fitting canvases and all data for removed gates or the owning session.
// Inputs: Gate-only selector. Outputs: Released contexts; unrelated fitter sessions remain active.
void PhotoPeakBridge::Close(bool gatedOnly) {
  fFits.erase(std::remove_if(fFits.begin(), fFits.end(), [&](auto& fit) {
    if(gatedOnly && fit->total) return false;
    auto* canvas = fit->canvas; fit->canvas = nullptr;
    if(canvas) { if(fApi) fApi->stop(canvas); canvas->Clear(); delete canvas; }
    return true;
  }), fFits.end());
}

// ============== RecursiveRemove ==============
// Purpose: Forget user-deleted canvases before subsequent cleanup. Inputs: Deleted object. Outputs: Safe borrowed identity.
void PhotoPeakBridge::RecursiveRemove(TObject* object) {
  for(auto& fit : fFits) if(fit) {
    if(fit->canvas == object) fit->canvas = nullptr;
    // ROOT may delete a snapshot through its native object menu.
    if(fit->spectrum.get() == object) { fit->spectrum.release(); fit->baseline.reset(); fit->stale = true; }
  }
}

} // namespace MatrixGate::Detail

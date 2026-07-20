#include <cmath>
#include <cstdio>

#include <Buttons.h>
#include <TCanvas.h>
#include <TH1D.h>
#include <TList.h>
#include <TObject.h>
#include <TROOT.h>

#include <PhotoPeakFit/PhotoPeakSession.h>

namespace {
int Fail(const char* message) {
  std::fprintf(stderr, "PhotoPeakSessionTest: %s\n", message);
  return 1;
}

int CountPhotoPeakMarkers(TCanvas& canvas) {
  int count = 0;
  TIter next(canvas.GetListOfPrimitives());
  while(TObject* object = next()) {
    if(object->GetUniqueID() == 0x50500001)
      ++count;
  }
  return count;
}
}

int main() {
  gROOT->SetBatch(true);
  TCanvas canvas("session_test", "session_test", 800, 600);
  TH1D histogram("session_hist", "session_hist", 400, 0.0, 400.0);
  for(int bin = 1; bin <= histogram.GetNbinsX(); ++bin) {
    const double x = histogram.GetBinCenter(bin);
    histogram.SetBinContent(bin, 100.0 + 10000.0 *
      std::exp(-0.5 * std::pow((x - 192.0) / 1.0, 2.0)));
  }
  histogram.Draw();
  canvas.Update();
  PhotoPeakSession session(&canvas, &canvas, &histogram);

  PhotoPeakInputEvent click;
  click.type = kButton1Down;
  click.x = 175.0;
  if(!session.HandleEvent(click))
    return Fail("left click was not consumed");
  click.x = 210.0;
  session.HandleEvent(click);
  click.x = 180.0;
  session.HandleEvent(click);
  if(std::fabs(session.State().request.fitLow - 180.0) > 1.0e-9 ||
     std::fabs(session.State().request.fitHigh - 210.0) > 1.0e-9)
    return Fail("third range click did not replace the first endpoint");

  session.AddPeak(200.0);
  session.AddPeak(190.0);
  if(session.State().request.peaks[0].centroid.value != 190.0)
    return Fail("peak seeds were not sorted");
  PhotoPeakInputEvent shift;
  shift.type = kButton1Shift;
  shift.x = 195.0;
  shift.px = canvas.XtoAbsPixel(shift.x);
  session.HandleEvent(shift);
  shift.type = kButton1Up;
  session.HandleEvent(shift);
  if(session.State().request.peaks.size() != 3)
    return Fail("shift click did not add a centroid marker");
  shift.type = kButton1Shift;
  session.HandleEvent(shift);
  shift.type = kButton1Up;
  session.HandleEvent(shift);
  if(session.State().request.peaks.size() != 2)
    return Fail("shift click did not toggle an existing centroid marker");
  PhotoPeakInputEvent unknown;
  unknown.type = 9999;
  if(!session.HandleEvent(unknown))
    return Fail("unknown interaction was not handled safely");
  session.Close();
  if(!session.IsClosed())
    return Fail("session did not close");
  if(CountPhotoPeakMarkers(canvas) == 0)
    return Fail("session close did not retain interactive markers");
  session.Resume();
  if(session.IsClosed())
    return Fail("session did not resume");
  session.Clean();
  if(CountPhotoPeakMarkers(canvas) != 0)
    return Fail("session cleanup did not remove retained markers");
  return 0;
}

#ifndef __PHOTOPEAKCONTROLWINDOW_H__
#define __PHOTOPEAKCONTROLWINDOW_H__

#include <memory>

#include <TGFrame.h>

class PhotoPeakSession;

class PhotoPeakControlWindow : public TGMainFrame {
  public:
    explicit PhotoPeakControlWindow(PhotoPeakSession* session);
    ~PhotoPeakControlWindow() override;

    Bool_t ProcessMessage(Long_t message, Long_t parameter1,
                          Long_t parameter2) override;
    void CloseWindow() override;
    void Refresh();
    void Show();
    void Hide();

  private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};

#endif

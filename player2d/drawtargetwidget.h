#ifndef DRAWTARGETWIDGET_H
#define DRAWTARGETWIDGET_H

#include <QWidget>
#ifdef WIN32
#include <d3d10_1.h>
#endif

struct ID3D10Device1;
struct IDXGISwapChain;

#include "2D.h"

class MainWindow;

class DrawTargetWidget : public QWidget
{
  Q_OBJECT
public:
  explicit DrawTargetWidget(QWidget *parent = 0);

  void InitDT();

  void SetMainWindow(MainWindow *aWindow) { mMainWindow = aWindow; }

#ifdef WIN32
  bool winEvent(MSG* message, long* result);
#endif
  void redraw();
  void paintEvent(QPaintEvent *event);
  virtual QPaintEngine *paintEngine() const { return nullptr; }
  virtual void resizeEvent(QResizeEvent * aEvent);

  mozilla::gfx::DrawTarget *GetDT() { return mDT; }
    
signals:
  void RefillDT();

private:
  mozilla::RefPtr<mozilla::gfx::DrawTarget> mDT;
#ifdef WIN32
  mozilla::RefPtr<IDXGISwapChain> mSwapChain;
#else
  unsigned char* mDTData;
#endif
  MainWindow *mMainWindow;
};

#endif // DRAWTARGETWIDGET_H

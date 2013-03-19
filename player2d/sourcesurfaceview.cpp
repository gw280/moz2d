#include "sourcesurfaceview.h"
#include "ui_sourcesurfaceview.h"
#include "2D.h"
#include "RecordedEvent.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace std;

SourceSurfaceView::SourceSurfaceView(ReferencePtr aRefPtr, mozilla::gfx::Translator *aTranslator, QWidget *parent) :
    SurfaceView(parent),
    ui(new Ui::SourceSurfaceView),
    mRefPtr(aRefPtr),
    mTranslator(aTranslator)
{
    ui->setupUi(this);
    ui->dtWidget->InitDT();

    connect(ui->dtWidget, SIGNAL(RefillDT()), SLOT(UpdateView()));
}

SourceSurfaceView::~SourceSurfaceView()
{
    delete ui;
}

TemporaryRef<SourceSurface>
SourceSurfaceView::GetSourceSurface()
{
  RefPtr<SourceSurface> surf = mTranslator->LookupSourceSurface(mRefPtr);
  return surf;
}

DrawTargetWidget*
SourceSurfaceView::GetDestDTWidget() const
{
  return ui->dtWidget;
}

QScrollBar*
SourceSurfaceView::GetHorizontalScrollBar() const
{
  return ui->horizontalScrollBar;
}

QScrollBar*
SourceSurfaceView::GetVerticalScrollBar() const
{
  return ui->verticalScrollBar;
}

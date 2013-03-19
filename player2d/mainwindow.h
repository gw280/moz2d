#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "playbackmanager.h"
#include "displaymanager.h"
#include "TreeItems.h"

namespace Ui {
class MainWindow;
}

class DrawTargetWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    DrawTargetWidget *GetDTWidget();

    void DefaultArrangement();
    void UpdateObjects();
    void FilterToObject(mozilla::gfx::ReferencePtr aObject);

    virtual void resizeEvent(QResizeEvent *);
signals:
    void UpdateViews();
    void EventChange();

private slots:
    void on_actionOpen_Recording_activated();

    void on_treeWidget_itemSelectionChanged();

    void on_objectTree_itemDoubleClicked(QTreeWidgetItem *item, int column);

    void on_viewWidget_tabCloseRequested(int index);

    void on_actionExit_triggered();

    void on_actionAnalyze_Redundancy_triggered();

    void UpdateEventColor(int32_t aID);

    void on_actionBack_triggered();

    void on_actionForward_triggered();

    void on_lineEdit_textChanged(const QString &arg1);

    void on_pushButton_clicked();

    void on_comboBox_currentIndexChanged(int index);

    void ObjectContextMenu(const QPoint &aPoint);

    void FilterByCurrentObject();
private:
    static ID3D10Device1 *sDevice;

    Ui::MainWindow *ui;
    PlaybackManager mPBManager;
    DisplayManager mDPManager;
    std::vector<QTreeWidgetItem*> mEventItems;

  std::vector<uint32_t> mEventPlayHistory;
  hash_set<void*> mObjects;
  int32_t mCurrentHistoryPosition;
  bool mAutomatedItemChange;
};

#endif // MAINWINDOW_H

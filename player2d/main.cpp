#include <QApplication>
#include "mainwindow.h"

#include "drawtargetwidget.h"
#include <qfile.h>
#include <QtPlugin>

#ifdef QT_STATIC
Q_IMPORT_PLUGIN(qico)
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    w.show();
    w.DefaultArrangement();

    Q_INIT_RESOURCE(resources);
  
    return a.exec();
}

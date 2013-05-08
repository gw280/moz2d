#-------------------------------------------------
#
# Project created by QtCreator 2012-08-06T16:20:06
#
#-------------------------------------------------

QT       += core gui

TARGET = player2d
TEMPLATE = app

SOURCES += main.cpp\
        mainwindow.cpp \
    drawtargetwidget.cpp \
    playbackmanager.cpp \
    displaymanager.cpp \
    drawtargetview.cpp \
    TreeItems.cpp \
    surfaceview.cpp \
    sourcesurfaceview.cpp \
    gradientstopsview.cpp \
    redundancyanalysis.cpp

HEADERS  += mainwindow.h \
    drawtargetwidget.h \
    playbackmanager.h \
    TreeItems.h \
    displaymanager.h \
    drawtargetview.h \
    surfaceview.h \
    sourcesurfaceview.h \
    gradientstopsview.h \
    redundancyanalysis.h

FORMS    += mainwindow.ui \
    drawtargetview.ui \
    sourcesurfaceview.ui \
    gradientstopsview.ui \
    redundancyanalysis.ui

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../release/ -lgfx2d
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../debug/ -lgfx2d
else:symbian: LIBS += -lgfx2d
else:unix: LIBS += -L`echo \$$PWD`/../ `echo \$$MOZ2D_PLAYER2D_LIBS` -lmoz2d

INCLUDEPATH += $$PWD/../
DEPENDPATH += $$PWD/../

static: QTPLUGIN += qico
static: DEFINES += QT_STATIC

win32:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../release/gfx2d.lib
else:win32:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../debug/gfx2d.lib
else:unix:!symbian: PRE_TARGETDEPS += $$PWD/../../libgfx2d.a

win32: LIBS += -ld3d10_1
win32: DEFINES += INITGUID

RESOURCES += \
    resources.qrc

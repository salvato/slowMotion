QT += core
QT += gui


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets


TARGET = slowMotion
TEMPLATE = app


DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000


SDKSTAGE = /home/pi/vc


CONFIG += c++14


SOURCES += main.cpp
SOURCES += setupdialog.cpp
SOURCES += picamera.cpp
SOURCES += maindialog.cpp
SOURCES += preview.cpp
SOURCES += commonsettings.cpp
SOURCES += cameracontrol.cpp


INCLUDEPATH += $$SDKSTAGE/include/
INCLUDEPATH+=-I$(SDKSTAGE)/include/interface/vcos/pthreads
INCLUDEPATH+=-I$(SDKSTAGE)/include/interface/vmcs_host/linux

INCLUDEPATH += /usr/local/include


HEADERS += maindialog.h
HEADERS += setupdialog.h
HEADERS += picamera.h
HEADERS += preview.h
HEADERS += commonsettings.h
HEADERS += cameracontrol.h


FORMS += maindialog.ui
FORMS += setupdialog.ui


LIBS+= -L$$SDKSTAGE/lib


LIBS += -lbcm_host
LIBS += -lvcos
LIBS += -lmmal
LIBS += -lmmal_core
LIBS += -lmmal_util

LIBS += -L"/usr/local/lib" -lpigpiod_if2


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    movie.png
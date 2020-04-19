QT += core
QT += gui
QT += widgets


TARGET = slowMotion
TEMPLATE = app


DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

# Libraries libEGL.so and libGLESv2.so
# present in /opt/vc are not compatible with QT

SDKSTAGE = /home/pi/vc


CONFIG += c++14


SOURCES += main.cpp
SOURCES += utility.cpp
SOURCES += jpegencoder.cpp
SOURCES += picamera.cpp
SOURCES += maindialog.cpp
SOURCES += preview.cpp
SOURCES += cameracontrol.cpp


INCLUDEPATH += -I $$SDKSTAGE/include
#INCLUDEPATH += /usr/local/include


HEADERS += maindialog.h
HEADERS += utility.h
HEADERS += jpegencoder.h
HEADERS += picamera.h
HEADERS += preview.h
HEADERS += cameracontrol.h


FORMS += maindialog.ui


LIBS+= -L $$SDKSTAGE/lib

LIBS += -lbcm_host
LIBS += -lvcos
LIBS += -lmmal
LIBS += -lmmal_core
LIBS += -lmmal_util

LIBS += -L"/usr/local/lib"
LIBS += -lpigpiod_if2


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += movie.png

QT       += core gui  webkitwidgets network widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

LIBS+= -lopencv_core -lopencv_imgproc  -lopencv_videoio
LIBS+= -lavcodec -lavformat -lavdevice -lavutil -lm

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    annotationwidget.cpp \
    main.cpp \
    mainwindow.cpp \
    movablerectitem.cpp \
    qgraphicsvideoitem.cpp

HEADERS += \
    annotationwidget.h \
    mainwindow.h \
    movablerectitem.h \
    qgraphicsvideoitem.h

# Default rules for deployment.
#qnx: target.path = /tmp/$${TARGET}/bin
#else: unix:!android: target.path = /opt/$${TARGET}/bin
#!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc

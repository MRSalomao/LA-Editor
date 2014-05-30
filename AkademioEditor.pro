TARGET = AkademioEditor
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++0x

CONFIG      +=  mobility
MOBILITY     =  multimedia

#INCLUDEPATH += /usr/include/QtMultimediaKit
#INCLUDEPATH += /usr/include/QtMobility
#QT += multimedia

QT          +=  core gui opengl widgets

SOURCES     +=  main.cpp\
                mainwindow.cpp \
                canvas.cpp \
                strokerenderer.cpp \
                shader.cpp \
                timeline.cpp \
    options.cpp

HEADERS     +=  mainwindow.h \
                canvas.h \
                strokerenderer.h \
                shader.h \
                configs.h \
                timeline.h \
                events.h \
    options.h

FORMS       +=  mainwindow.ui \
    options.ui

RESOURCES   +=  resources.qrc

OTHER_FILES +=  shaders/*

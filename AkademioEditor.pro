TARGET = AkademioEditor
TEMPLATE = app

# Use qt4 version of qt-sdk
linux-g++ {
    CONFIG          += mobility
    MOBILITY         = multimedia
    QMAKE_CXXFLAGS  += -std=c++0x
}

# Use qt 4.8.5 - MinGW4.8 (4.8.6 has wacom bugs) (need to build from source)
win32 {
    QMAKE_CXXFLAGS  += -std=c++11
    CONFIG          += c++11
    QT              += multimedia
}

# Use qt 5.3.0 (previous versions include bugs like not changing the cursor type)
macx {
    QMAKE_CXXFLAGS  += -stdlib=libc++
    QMAKE_CXXFLAGS  += -std=c++11
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.7
    QT              += widgets multimedia
}

QT          +=  core gui opengl

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

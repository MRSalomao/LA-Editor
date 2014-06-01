TARGET = AkademioEditor
TEMPLATE = app

equals(QT_MAJOR_VERSION, 4) {
    CONFIG          +=  mobility
    MOBILITY         =  multimedia
    QMAKE_CXXFLAGS  += -std=c++0x
}

equals(QT_MAJOR_VERSION, 5) {
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

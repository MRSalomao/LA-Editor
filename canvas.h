#ifndef TABLETCANVAS_H
#define TABLETCANVAS_H

#include <QGLWidget>
#include <QPixmap>
#include <QPoint>
#include <QTabletEvent>
#include <QMouseEvent>
#include <QColor>
#include <QBrush>
#include <QPen>
#include <QPoint>
#include <QPainter>

#include <QTime>
#include <QTimer>

#include "strokerenderer.h"

#if QT_VERSION >= 0x050000
    #define EVENT_POSF event->posF();
#else
    #define EVENT_POSF event->hiResGlobalPos() - mapToGlobal(QPoint(0,0));
#endif

class Canvas : public QGLWidget, protected OPENGL_FUNCTIONS
{
    Q_OBJECT

    QTimer fpsTimer;
    QTime time;
    int frames = 0;

    bool deviceDown = false;

    void updateFPS();

    void rescalePenPos();

public:
    Canvas(QWidget* parent);
    ~Canvas();

    int w, h, totalH;

    static Canvas* si;

    StrokeRenderer strokeRenderer;

    bool redrawRequested = false;
    bool incrementalDrawRequested = false;
    bool pickingRequested = false;

    void clearScreen();

    GLuint canvasFramebufferID = -1;
    GLuint pickingFramebufferID = -1;
    GLuint canvasTextureID = -1;
    GLuint pickingTextureID = -1;

    QPointF penPos, lastPenPos;
    QPoint penIntPos, lastPenIntPos;

protected:
    void paintGL ();
    void initializeGL ();
    void resizeGL(int w, int h);

    void tabletEvent(QTabletEvent *event);
    void mouseMoveEvent( QMouseEvent * event );
    void mousePressEvent( QMouseEvent * event );
    void mouseReleaseEvent( QMouseEvent * event );
    void wheelEvent(QWheelEvent * event);

};

#endif

#include <QtGui>
#include <QApplication>
#include <math.h>
#include <qmath.h>
#include "canvas.h"
#include "timeline.h"
#include "events.h"

Canvas* Canvas::si;

QGLFormat desiredFormat()
{
    QGLFormat fmt;
//    fmt.setSwapInterval(1);
    return fmt;
}

Canvas::Canvas(QWidget *parent)
    : QGLWidget(/*desiredFormat(),*/ parent), frames(0)
{
    si = this;

    this->makeCurrent();

    setCursor(QCursor(QPixmap(":/icons/icons/cursor32.png")));

//    connect(&fpsTimer, SIGNAL(timeout()), this, SLOT(updateGL()));
//    if(format().swapInterval() == -1)
//    {
//        qDebug() << "V_blank synchronization not available (tearing might be noticed)";
//        qDebug() << "Refresh at approx 60fps.";
//        fpsTimer.setInterval(17);
//    }
//    else
//    {
//        qDebug() << "V_blank synchronization available";
//        qDebug() << "Swap interval = " << format().swapInterval();
//        fpsTimer.setInterval(0);
//    }
//    fpsTimer.start();
}

void Canvas::initializeGL()
{
    INIT_OPENGL_FUNCTIONS();

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DITHER);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_POINT_SPRITE);
    glPointSize(3);

    int ib[2];
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, ib);
    qDebug() << "Framebuffer max size: " << ib[0] << ib[1];

    strokeRenderer.init();
}

void Canvas::paintGL()
{
    clearScreen();

    if (redrawRequested)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, canvasFramebufferID);

        clearScreen();

        Timeline::si->redrawScreen();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        redrawRequested = false;
    }
    else if (incrementalDrawRequested || Timeline::si->isPlaying)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, canvasFramebufferID);

        Timeline::si->incrementalDraw();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        incrementalDrawRequested = false;
    }

    //Draw the fbo
    glBindTexture(GL_TEXTURE_2D, canvasTextureID);
    strokeRenderer.drawTexturedRect(0,0,1,1);
    glBindTexture(GL_TEXTURE_2D, 0);

    //If playing the video, draw a cursor:
    if(Timeline::si->isPlaying)
    {
        strokeRenderer.drawCursor();
    }

//    updateFPS();
    if (!MainWindow::si->fileDialogOpen) update();
}

void Canvas::tabletEvent(QTabletEvent *event)
{
    penPos = EVENT_POSF;

//    penPos = ;

    int pbo;

    switch (event->type())
    {
        case QEvent::TabletPress:
            if (deviceDown) return;
            deviceDown = true;
            pbo = strokeRenderer.addPoint(penPos);
            Timeline::si->addPointerEndEvent();
            Timeline::si->addStrokeBeginEvent(penPos, strokeRenderer.getCurrentSpriteCounter(), pbo);
            break;


        case QEvent::TabletRelease:
            if (!deviceDown) return;
            deviceDown = false;
            Timeline::si->addStrokeEndEvent();
            Timeline::si->addPointerBeginEvent(penPos);
            break;


        case QEvent::TabletMove:
            if (deviceDown)
            {
                pbo = strokeRenderer.addStroke(QLineF(lastPenPos, penPos));
                Timeline::si->addStrokeMoveEvent(penPos, pbo);
            }
            else
            {
                Timeline::si->addPointerMoveEvent(penPos);
            }
            break;

        default:
            break;
    }
    event->accept(); qDebug() << event->posF();

    lastPenPos = penPos;
}

void Canvas::mousePressEvent(QMouseEvent *event)
{
    if (deviceDown) return;
    penPos = event->pos();
    deviceDown = true;

    int pbo = strokeRenderer.addPoint(penPos);
    lastPenPos = penPos;

    Timeline::si->addPointerEndEvent();
    Timeline::si->addStrokeBeginEvent(penPos, strokeRenderer.getCurrentSpriteCounter(), pbo);
}

void Canvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (!deviceDown) return;
    penPos = event->pos();
    deviceDown = false;

    lastPenPos = penPos;

    Timeline::si->addStrokeEndEvent();
    Timeline::si->addPointerBeginEvent(penPos);
}

void Canvas::mouseMoveEvent(QMouseEvent *event)
{
    penPos = event->pos();

    if (deviceDown)
    {
        int pbo = strokeRenderer.addStroke(QLineF(lastPenPos, penPos));

        Timeline::si->addStrokeMoveEvent(penPos, pbo);
    }
    else
    {
        Timeline::si->addPointerMoveEvent(penPos);
    }
    lastPenPos = penPos;
}

#define scrollSensitivity 40
void Canvas::wheelEvent(QWheelEvent *event)
{
    MainWindow::si->changeCanvasScrollBar(-event->delta() / scrollSensitivity);
}

Canvas::~Canvas()
{
}

void Canvas::resizeGL(int w, int h)
{
    this->w = w;
    this->h = h;

    strokeRenderer.windowSizeChanged(w,h);

    glViewport(0, 0, w, h);

    if(canvasFramebufferID == -1)
    {
        glGenFramebuffers(1, &canvasFramebufferID);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, canvasFramebufferID);

    if(canvasTextureID == -1)
    {
        glGenTextures(1, &canvasTextureID);
    }
    glBindTexture(GL_TEXTURE_2D, canvasTextureID);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Create a layer for our canvas
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, canvasTextureID, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        qDebug("Canvas framebuffer not created.");
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    clearScreen();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Canvas::clearScreen()
{
    glClear(GL_COLOR_BUFFER_BIT);
}

void Canvas::updateFPS()
{
    if ( !(frames % (60 * 3) ) )
    {
        QString framesPerSecond;
        framesPerSecond.setNum(frames /(time.elapsed() / 1000.0), 'f', 2);

        time.start();

        frames = 0;

        qDebug() << framesPerSecond + " fps";
    }

    frames ++;
}

#include "strokerenderer.h"
#include "timeline.h"
#include <limits>

#include <vector>


#define VERTEX_COORD_SIZE 4
#define N_SPRITES 10000000

StrokeRenderer* StrokeRenderer::si;

StrokeRenderer::StrokeRenderer()
{
    si = this;
}

void StrokeRenderer::windowSizeChanged(int width, int height)
{
    qDebug() << "canvas resized to:" << width << height;

    canvasSize.setX(width);
    canvasSize.setY(height);

    canvasViewportRatio = canvasSize.y() / canvasSize.x();

    yMultiplier = canvasViewportRatio / canvasRatio;

    strokeShader.shaderProgram.bind();
    strokeShader.shaderProgram.setUniformValue(strokeSPLoc, 1.0 / yMultiplier, viewportYCenter);

    MainWindow::si->getCanvasScrollBar()->setMaximum(99 - 99 * yMultiplier);
    MainWindow::si->getCanvasScrollBar()->setMinimum(0);
    MainWindow::si->getCanvasScrollBar()->setPageStep(99 * yMultiplier);

    Canvas::si->redrawRequested = true;
}

void StrokeRenderer::setViewportYCenter(float value)
{
    viewportYCenter = value * (1.0f / yMultiplier - 1.0f) / MainWindow::si->getCanvasScrollBar()->maximum() ;

    strokeShader.shaderProgram.bind();
    strokeShader.shaderProgram.setUniformValue(strokeSPLoc, 1.0 / yMultiplier, viewportYCenter);

    Canvas::si->redrawRequested = true;
}

void StrokeRenderer::init()
{
    INIT_OPENGL_FUNCTIONS();

    strokeShader.init(QString("stroke"), {"inTexCoord"});

    glGenBuffers(1, &verticesId);
    glBindBuffer(GL_ARRAY_BUFFER, verticesId);
    glBufferData(GL_ARRAY_BUFFER, N_SPRITES, NULL, GL_STREAM_DRAW);

    strokeShader.shaderProgram.bind();
    strokeColorLoc = strokeShader.shaderProgram.uniformLocation("strokeColor");
    strokeSPLoc = strokeShader.shaderProgram.uniformLocation("scrollProperties");

    strokeShader.shaderProgram.setUniformValue(strokeColorLoc, 0.0, 0.0, 0.0);

    canvasShader.init(QString("canvas"), {});

    glGenBuffers(1, &rectId);
    glBindBuffer(GL_ARRAY_BUFFER, rectId);
    glBufferData(GL_ARRAY_BUFFER, 4, NULL, GL_DYNAMIC_DRAW);

    canvasShader.shaderProgram.bind();
    samplerRectLoc = canvasShader.shaderProgram.uniformLocation("sampler");

    canvasShader.shaderProgram.setUniformValue(samplerRectLoc, 0);


    QImage cursor = QGLWidget::convertToGLFormat( QImage(":/icons/icons/cursor32.png") );

    glGenTextures( 1, &cursorTexture );
    glBindTexture( GL_TEXTURE_2D, cursorTexture );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, cursor.width(), cursor.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, cursor.bits() );

    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
}

int StrokeRenderer::addStroke(const QLineF &strokeLine)
{
    float w = strokeLine.x2() - strokeLine.x1();
    float h = strokeLine.y2() - strokeLine.y1();

    float dist = sqrt(h*h + w*w);

    float i;
    for (i=extraDist; i<dist; i++)
    {
        addStrokeSprite(strokeLine.x1() + w / dist * i,
                        strokeLine.y1() + h / dist * i);
    }
    extraDist = i-dist;

    return spriteCounter;
}


int StrokeRenderer::addPoint(const QPointF &strokePoint)
{
    addStrokeSprite(strokePoint.x(), strokePoint.y());

    return spriteCounter;
}


int StrokeRenderer::getCurrentSpriteCounter()
{
    return spriteCounter;
}


void StrokeRenderer::addStrokeSprite(float x, float y)
{
    x /= canvasSize.x();
    y /= canvasSize.y();
    y += viewportYCenter * 0.5f;
    y *= yMultiplier;

    if (x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0) return;

    if(spriteCounter < N_SPRITES)
    {
        GLushort posArray[] = {x * USHRT_MAX, y * USHRT_MAX};

        glBindBuffer(GL_ARRAY_BUFFER, verticesId);
        glBufferSubData(GL_ARRAY_BUFFER, spriteCounter*VERTEX_COORD_SIZE, VERTEX_COORD_SIZE, posArray);

        spriteCounter++;
    }
}

//void StrokeRenderer::drawStrokeSprites()
//{
//    glUseProgram(strokeShader.pId);

//    glBindBuffer(GL_ARRAY_BUFFER, verticesId);
//    glVertexAttribPointer(0, 2, GL_UNSIGNED_SHORT, true, VERTEX_COORD_SIZE, 0);

//    glEnableVertexAttribArray(0);

//    glDrawArrays(GL_POINTS, 0, spriteCounter);

//    glDisableVertexAttribArray(0);
//}

void StrokeRenderer::drawCursor()
{
    glBindTexture(GL_TEXTURE_2D, cursorTexture);
    drawTexturedRect(Timeline::si->cursorPosition.x() / canvasSize.x() - 32.0 / 2 / canvasSize.x(),
                     Timeline::si->cursorPosition.y() / canvasSize.y() - 32.0 / 2 / canvasSize.y(),
                     32.0/ canvasSize.x(),
                     32.0/ canvasSize.y() );
}


void StrokeRenderer::drawStrokeSpritesRange(int from, int to, float r, float g, float b, float ptSize, QMatrix4x4 transform)
{
    glPointSize(ptSize);

    glUseProgram(strokeShader.pId);
    strokeShader.shaderProgram.setUniformValue(strokeColorLoc, r, g, b);

    glBindBuffer(GL_ARRAY_BUFFER, verticesId);
    glVertexAttribPointer(0, 2, GL_UNSIGNED_SHORT, true, VERTEX_COORD_SIZE, 0);

    glEnableVertexAttribArray(0);

    glDrawArrays(GL_POINTS, from, to - from);

    glDisableVertexAttribArray(0);
}


float StrokeRenderer::getStrokeSize() const
{
    return strokeSize;
}

void StrokeRenderer::setStrokeSize(float value)
{
    strokeSize = value;

    glPointSize(strokeSize);
}

QColor StrokeRenderer::getActiveColor() const
{
    return activeColor;
}

void StrokeRenderer::setActiveColor(const QColor &value)
{
    activeColor = value;
}

void StrokeRenderer::drawTexturedRect(float x, float y, float w, float h)
{
    float posArray[] = {x,   y,   0, 1,
                        x,   y+h, 0, 0,
                        x+w, y+h, 1, 0,
                        x+w, y,	  1, 1};

    glUseProgram(canvasShader.pId);

    glBindBuffer(GL_ARRAY_BUFFER, rectId);
    glBufferData(GL_ARRAY_BUFFER, 64, posArray, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, false, 16, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, false, 16, (void*)8);
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

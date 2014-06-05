#ifndef STROKERENDERER_H
#define STROKERENDERER_H

#include <qscrollbar.h>
#include <QColor>

#if QT_VERSION >= 0x050000
    #include <QOpenGLFunctions>
    #define INIT_OPENGL_FUNCTIONS initializeOpenGLFunctions
    #define OPENGL_FUNCTIONS QOpenGLFunctions
#else
    #include <QGLFunctions>
    #define INIT_OPENGL_FUNCTIONS initializeGLFunctions
    #define OPENGL_FUNCTIONS QGLFunctions
#endif

#include "shader.h"

class StrokeRenderer : protected OPENGL_FUNCTIONS
{
    int strokeColorLoc, zoomAndScrollLoc;
    int samplerRectLoc;

    float extraDist = 0;
    int spriteCounter = 0;
    float strokeSize;
    QColor activeColor;

    float zoom, scroll;

    GLuint verticesId;
    GLuint rectId;

    Shader strokeShader;
    Shader canvasShader;

    QPointF canvasSize;

    GLuint cursorTexture;

public:
    StrokeRenderer();

    static StrokeRenderer* si;

    void windowSizeChanged(int width, int height);

    void init();

    int addStroke(const QLineF &strokeLine);
    int addPoint(const QPointF &strokePoint);

    void addStrokeSprite(float x, float y);

    const float canvasRatio = 2.0;
    const float canvasRatioSquared = canvasRatio * canvasRatio;
    float viewportYStart = 0;

    const float spriteSpacing = 1.0 / 250.0;

    float scrollBarSize = 100;
    QScrollBar* scrollBar;

    //getters and setters
    QColor getActiveColor() const;
    void setActiveColor(const QColor &value);
    float getStrokeSize() const;
    void setStrokeSize(float value);

    int getCurrentSpriteCounter();
    void drawStrokeSpritesRange(int from, int to, float r, float g, float b, float ptSize, QMatrix4x4 transform);
    void drawTexturedRect(float x, float y, float w, float h);
    void setViewportYStart(float value);
    void drawCursor();
};

#endif

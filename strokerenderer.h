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
    int strokeColorLoc, strokeZoomAndScrollLoc, strokeMatrix;
    int pickingColorLoc, pickingZoomAndScrollLoc, pickingMatrix;
    int rectZoomAndScrollLoc;
    int samplerRectLoc;

    float extraDist = 0;
    int spriteCounter = 0;

    float zoom, scroll;

    GLuint verticesId;
    GLuint rectId;

    Shader strokeShader;
    Shader pickingShader;
    Shader selectionRectShader;
    Shader canvasShader;

    QPointF canvasSize;

    GLuint cursorTexture;

    float normalSizeAdjustment = 1.0f / 542.0f;
    float pickingSizeAdjustment = 5.0f;

public:
    StrokeRenderer();

    static StrokeRenderer* si;

    void windowSizeChanged(int width, int height);

    void init();

    int addStroke(const QLineF &strokeLine);
    int addPoint(const QPointF &strokePoint);

    void processPicking();

    void renderSelectionRect(QRectF rect);

    void addStrokeSprite(float x, float y);

    const float canvasRatio = 2.0f;
    const float canvasRatioSquared = canvasRatio * canvasRatio;
    float viewportYStart = 0;

    const float spriteSpacing = SHRT_MAX / 250.0f;

    float scrollBarSize = 100;
    QScrollBar* scrollBar;

    int getCurrentSpriteCounter();
    void drawStrokeSpritesRange(int from, int to, float r, float g, float b, float ptSize, QMatrix4x4 transform, int ID);
    void drawTexturedRect(float x, float y, float w, float h);
    void setViewportYStart(float value);
    void drawCursor();
};

#endif

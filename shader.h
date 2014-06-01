#ifndef SHADER_H
#define SHADER_H

#include <QtCore>

#if QT_VERSION >= 0x050000
    #include <QOpenGLShaderProgram>
    #define SHADER_PROGRAM QOpenGLShaderProgram
    #define SHADER QOpenGLShader
#else
    #include <QGLShaderProgram>
    #include <QGLShader>
    #define SHADER_PROGRAM QGLShaderProgram
    #define SHADER QGLShader
#endif


class Shader
{
public:
    Shader();

    SHADER_PROGRAM shaderProgram;

    int pId;

    void init(QString shader, QVector<QString> attributes);

    Shader(QString vshader, QVector<QString> attributes);
private:

};

#endif

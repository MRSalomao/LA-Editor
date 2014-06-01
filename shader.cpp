#include "shader.h"
#include "configs.h"

#include <QFileInfo>

Shader::Shader(QString shader, QVector<QString> attributes)
{
    init(shader, attributes);
}

Shader::Shader()
{

}

void Shader::init(QString shader, QVector<QString> attributes)
{
    QString vertexShaderFilename(EXEC_FOLDER + "shaders/" + shader + ".vsh");
    QString fragmentShaderFilename(EXEC_FOLDER + "shaders/" + shader + ".fsh");

//    // load and compile vertex shader
    QFileInfo vsh(vertexShaderFilename);
    if(vsh.exists())
    {
        SHADER* vertexShader = new SHADER(SHADER::Vertex);
        if(vertexShader->compileSourceFile(vertexShaderFilename))
            shaderProgram.addShader(vertexShader);
        else qWarning() << "Vertex Shader Error" << vertexShader->log();
    }
    else qWarning() << "Vertex Shader source file " << vertexShaderFilename << " not found.";

    //load and compile fragment shader
    QFileInfo fsh(fragmentShaderFilename);
    if(fsh.exists())
    {
        SHADER* fragmentShader = new SHADER(SHADER::Fragment);
        if(fragmentShader->compileSourceFile(fragmentShaderFilename))
            shaderProgram.addShader(fragmentShader);
        else qWarning() << "Fragment Shader Error" << fragmentShader->log();
    }
    else qWarning() << "Fragment Shader source file " << fragmentShaderFilename << " not found.";

    pId = shaderProgram.programId();

    shaderProgram.bindAttributeLocation("vertexPos", 0);

    for (int i = 0; i < attributes.size(); i++)
    {
        shaderProgram.bindAttributeLocation(attributes[i], i+1);
    }

    if(!shaderProgram.link())
    {
        qWarning() << "Shader Program Linker Error" << shaderProgram.log();
    }
}

void printGlError()
{
    GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR) qDebug("all good");
    else qDebug() << "error: " << glErr;
}

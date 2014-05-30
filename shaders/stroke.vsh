#version 120

attribute highp vec2 vertexPos;

uniform highp vec2 scrollProperties;

void main()
{
    gl_Position = vec4(vertexPos.xy * vec2(2.0, -2.0 * scrollProperties.x)
                       + vec2(-1.0, 1.0 + scrollProperties.y), 0.0, 1.0);
}

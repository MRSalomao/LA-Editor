attribute highp vec2 vertexPos;

uniform highp vec2 zoomAndScroll;

uniform highp mat4 manipulation;

void main()
{
    highp vec2 vertex = ( manipulation * vec4(vertexPos,0,1) ).xy;

    gl_Position = vec4(vertex.x, vertex.y * zoomAndScroll.x - zoomAndScroll.x + 1.0 + zoomAndScroll.y, 0.0, 1.0);
}

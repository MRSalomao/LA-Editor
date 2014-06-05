attribute highp vec2 vertexPos;

uniform highp vec2 zoomAndScroll;

void main()
{
    gl_Position = vec4(vertexPos.x, vertexPos.y * zoomAndScroll.x - zoomAndScroll.x + 1.0 + zoomAndScroll.y, 0.0, 1.0);
}

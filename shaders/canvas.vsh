attribute highp vec2 vertexPos;
attribute highp vec2 inTexCoord;

varying highp vec2 texCoord;

void main()
{
    texCoord = inTexCoord.xy;
	
    gl_Position = vec4(vertexPos.xy, 0.0, 1.0);
}

#version 120

#define PI 3.14159265

uniform highp vec3 strokeColor;


void main()
{
    vec2 pos = gl_PointCoord - vec2(0.5);

    float dst2 = dot(pos, pos);

    float alpha = 1.0 - step(0.25, dst2);

    gl_FragColor = vec4(strokeColor, alpha);
}

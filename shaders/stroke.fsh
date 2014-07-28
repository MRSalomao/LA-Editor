#version 120

#define PI 3.14159265

uniform highp vec3 strokeColor;

void main()
{
    vec2 pos = gl_PointCoord - vec2(0.5);

    float dst2 = dot(pos, pos);

    float alpha = (0.25 - min(dst2, 0.25)) * 2.7; // From 0, in the center, to 1, at the border. Linearly.

    gl_FragColor = vec4(strokeColor, alpha);
}

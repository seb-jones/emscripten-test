uniform float point_size;
attribute vec4 position;
attribute vec4 colour;
varying vec4 varying_colour

void main()
{
    gl_Position = position;
    gl_PointSize = point_size;
    varying_colour = colour;
}

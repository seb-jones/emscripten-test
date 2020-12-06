uniform vec4 translation;
attribute vec4 position;
attribute vec2 tex_coord;
varying vec2 tex_coord_v;

void main()
{
    gl_Position = position - translation;
    tex_coord_v = tex_coord;
}

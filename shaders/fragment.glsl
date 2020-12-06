precision mediump float;                   
varying vec2 tex_coord_v;
uniform sampler2D sampler;

void main()
{
    gl_FragColor = texture2D(sampler, tex_coord_v);
}

uniform vec2 translation;
attribute vec4 position;

void main()                 
{                           
    gl_Position.x = position.x - translation.x;
    gl_Position.y = position.y - translation.y;
    gl_Position.z = position.z;
    gl_Position.w = position.w;
}                           

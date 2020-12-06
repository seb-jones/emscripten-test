uniform vec4 translation;
attribute vec4 position;

void main()                 
{                           
    gl_Position = position - translation;
}                           

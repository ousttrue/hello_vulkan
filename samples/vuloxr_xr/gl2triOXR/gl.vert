attribute vec4 a_Vertex;
attribute vec4 a_Color;
varying vec4 v_color;

void main(void) {
  gl_Position = a_Vertex;
  v_color = a_Color;
}

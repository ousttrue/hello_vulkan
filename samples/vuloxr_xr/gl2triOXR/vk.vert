#version 430
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec4 a_Vertex;
layout (location = 1) in vec4 a_Color;

layout (location = 0) out vec4 v_color;
out gl_PerVertex
{
    vec4 gl_Position;
};

void main() {
  gl_Position = a_Vertex;
  v_color = a_Color;
}

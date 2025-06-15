#version 430
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec4 oColor;

layout (location = 0) out vec4 FragColor;

void main()
{
    FragColor = oColor;
}

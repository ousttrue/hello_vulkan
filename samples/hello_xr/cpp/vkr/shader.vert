#version 430
#extension GL_ARB_separate_shader_objects : enable

layout (std140, push_constant) uniform buf
{
    mat4 mvp;
} ubuf;

layout (location = 0) in vec3 Position;
layout (location = 1) in vec4 Color;

layout (location = 0) out vec4 oColor;
out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    oColor.rgba  = Color.rgba;
    gl_Position = ubuf.mvp * vec4(Position, 1);
}


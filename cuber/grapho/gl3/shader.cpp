#include "shader.h"

namespace grapho::gl3 {

std::optional<GLuint>
compile(GLenum shaderType, std::span<std::u8string_view> srcs)
{
  auto shader = glCreateShader(shaderType);

  std::vector<const GLchar*> string;
  std::vector<GLint> length;
  for (auto src : srcs) {
    string.push_back((const GLchar*)src.data());
    length.push_back(src.size());
  }
  glShaderSource(shader, srcs.size(), string.data(), length.data());
  glCompileShader(shader);
  GLint isCompiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
  if (isCompiled == GL_FALSE) {
    GLint maxLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

    // The maxLength includes the NULL character
    std::string errorLog;
    errorLog.resize(maxLength);
    glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

    glDeleteShader(shader); // Don't leak the shader.

    // Provide the infolog in whatever manor you deem best.
    // return std::unexpected{ errorLog };
    return {};
  }
  return shader;
}

std::optional<GLuint>
link(GLuint vs, GLuint fs, GLuint gs)
{
  GLuint program = glCreateProgram();

  // Attach shaders as necessary.
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  if (gs) {
    glAttachShader(program, gs);
  }

  // Link the program.
  glLinkProgram(program);

  GLint isLinked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
  if (isLinked == GL_FALSE) {
    GLint maxLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

    // The maxLength includes the NULL character
    std::string infoLog;
    infoLog.resize(maxLength);
    glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

    // The program is useless now. So delete it.
    glDeleteProgram(program);

    // Provide the infolog in whatever manner you deem best.
    // return std::unexpected{ infoLog };
    return {};
  }
  return program;
}

} // namespace

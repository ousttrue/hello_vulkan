#pragma once
#include <GL/glew.h>

#include "../fileutil.h"
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <stdint.h>
#include <string>
#include <string_view>

namespace grapho::gl3 {

std::optional<GLuint>
compile(GLenum shaderType, std::span<std::u8string_view> srcs);

std::optional<GLuint>
link(GLuint vs, GLuint fs, GLuint gs = 0);

template<typename T>
concept Float3 = sizeof(T) == sizeof(float) * 3;
template<typename T>
concept Float4 = sizeof(T) == sizeof(float) * 4;
template<typename T>
concept Mat3 = sizeof(T) == sizeof(float) * 9;
template<typename T>
concept Mat4 = sizeof(T) == sizeof(float) * 16;

struct UniformVariable
{
  uint32_t Location;
  std::string Name;
  GLenum Type;

  void Set(int value) const
  {
    //
    glUniform1i(Location, value);
  }

  void Set(float value) const { glUniform1f(Location, value); }

  // 12 byte
  template<Float3 T>
  void Set(const T& t) const
  {
    glUniform3fv(Location, 1, (const float*)&t);
  }
  // 16 byte
  template<Float4 T>
  void Set(const T& t) const
  {
    glUniform4fv(Location, 1, (const float*)&t);
  }
  // 36 byte
  template<Mat3 T>
  void Set(const T& t) const
  {
    glUniformMatrix3fv(Location, 1, GL_FALSE, (const float*)&t);
  }
  // 64 byte
  template<Mat4 T>
  void Set(const T& t) const
  {
    glUniformMatrix4fv(Location, 1, GL_FALSE, (const float*)&t);
  }
};

inline void
DebugWrite(const std::string& path, std::span<std::u8string_view> srcs)
{
#ifndef NDEBUG
  std::stringstream ss;
  for (auto src : srcs) {
    ss << std::string_view((const char*)src.data(), src.size());
  }
  std::ofstream os(path);
  os << ss.str();
#endif
}

class ShaderProgram
{
  uint32_t program_ = 0;

  ShaderProgram(uint32_t program)
    : program_(program)
  {
    // https://stackoverflow.com/questions/440144/in-opengl-is-there-a-way-to-get-a-list-of-all-uniforms-attribs-used-by-a-shade
    int count;
    glGetProgramiv(program_, GL_ACTIVE_UNIFORMS, &count);
    // printf("Active Uniforms: %d\n", count);
    Uniforms.reserve(count);

    for (GLuint i = 0; i < count; i++) {
      // maximum name length
      const GLsizei bufSize = 64;
      // variable name in GLSL
      GLchar name[bufSize];
      // size of the variable
      GLint size;
      // type of the variable (float, vec3 or mat4, etc)
      GLenum type;
      // name length
      GLsizei length;
      glGetActiveUniform(program_, i, bufSize, &length, &size, &type, name);

      auto location = glGetUniformLocation(program_, name);
      // assert(location != -1);

      Uniforms.push_back({
        .Location = (uint32_t)location,
        .Name = name,
        .Type = type,
      });
      //
      // if (location == -1) {
      // // UBO
      //   printf("Uniform #%d Location: %d, Type: %u Name: %s\n",
      //          i,
      //          location,
      //          type,
      //          name);
      // }
    }
  }

public:
  std::vector<UniformVariable> Uniforms;
  ~ShaderProgram() { glDeleteProgram(program_); }
  static std::shared_ptr<ShaderProgram> Create(
    std::span<std::u8string_view> vs_srcs,
    std::span<std::u8string_view> fs_srcs,
    std::span<std::u8string_view> gs_srcs = {})
  {
    auto vs = compile(GL_VERTEX_SHADER, vs_srcs);
    if (!vs) {
      DebugWrite("debug.vert", vs_srcs);
      // return std::unexpected{ std::string("vs: ") + vs.error() };
      return {};
    }
    auto fs = compile(GL_FRAGMENT_SHADER, fs_srcs);
    if (!fs) {
      DebugWrite("debug.frag", fs_srcs);
      // return std::unexpected{ std::string("fs: ") + fs.error() };
      return {};
    }

    GLuint gs = 0;
    if (!gs_srcs.empty()) {
      auto _gs = compile(GL_GEOMETRY_SHADER, gs_srcs);
      if (!_gs) {
        DebugWrite("debug.geom", gs_srcs);
        // return std::unexpected{ std::string("gs: ") + _gs.error() };
        return {};
      }
      gs = *_gs;
    }

    auto program = link(*vs, *fs, gs);
    if (!program) {
      // return std::unexpected{ program.error() };
      return {};
    }

    return std::shared_ptr<ShaderProgram>(new ShaderProgram(*program));
  }

  static std::shared_ptr<ShaderProgram> Create(std::u8string_view vs,
                                               std::u8string_view fs,
                                               std::u8string_view gs = {})
  {
    std::u8string_view vss[] = { vs };
    std::u8string_view fss[] = { fs };
    if (gs.empty()) {
      return Create(vss, fss);
    } else {
      std::u8string_view gss[] = { gs };
      return Create(vss, fss, gss);
    }
  }

  static std::shared_ptr<ShaderProgram> Create(std::string_view vs,
                                               std::string_view fs,
                                               std::string_view gs = {})
  {
    std::u8string_view vss[] = { { (const char8_t*)vs.data(),
                                   (const char8_t*)vs.data() + vs.size() } };
    std::u8string_view fss[] = { { (const char8_t*)fs.data(),
                                   (const char8_t*)fs.data() + fs.size() } };
    if (gs.empty()) {
      return Create(vss, fss);
    } else {
      std::u8string_view gss[] = { { (const char8_t*)gs.data(),
                                     (const char8_t*)gs.data() + gs.size() } };
      return Create(vss, fss, gss);
    }
  }

  static std::shared_ptr<ShaderProgram> CreateFromPath(
    const std::string& vs_path,
    const std::string& fs_path,
    const std::string& gs_path = {})
  {
    auto vs = grapho::StringFromPath(vs_path);
    auto fs = grapho::StringFromPath(fs_path);
    if (gs_path.empty()) {
      return Create(vs, fs);
    } else {
      auto gs = grapho::StringFromPath(gs_path);
      return Create(vs, fs, gs);
    }
  }

  void Use() { glUseProgram(program_); }
  void UnUse() { glUseProgram(0); }

  std::optional<uint32_t> Attribute(const char* name) const
  {
    auto location = glGetAttribLocation(program_, name);
    if (location < 0) {
      return std::nullopt;
    }
    return static_cast<uint32_t>(location);
  }

  std::optional<UniformVariable> Uniform(const std::string& name) const
  {
    auto location = glGetUniformLocation(program_, name.c_str());
    if (location < 0) {
      return std::nullopt;
    }
    return UniformVariable{ static_cast<uint32_t>(location) };
  }

  template<typename T>
  void SetUniform(const std::string& name, const T& value) const
  {
    if (auto var = Uniform(name)) {
      var->Set(value);
    }
  }

  void SetUniform(const std::string& name, int value) const
  {
    if (auto var = Uniform(name)) {
      var->Set(value);
    }
  }

  void SetUniform(const std::string& name, float value) const
  {
    if (auto var = Uniform(name)) {
      var->Set(value);
    }
  }

  std::optional<uint32_t> UboBlockIndex(const char* name)
  {
    auto blockIndex = glGetUniformBlockIndex(program_, name);
    if (blockIndex < 0) {
      return std::nullopt;
    }
    return blockIndex;
  }

  void UboBind(uint32_t blockIndex, uint32_t binding_point)
  {
    glUniformBlockBinding(program_, blockIndex, binding_point);
  }
};

}

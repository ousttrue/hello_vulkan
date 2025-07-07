#pragma once
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace grapho {

enum class ShaderTypes
{
  vec2,
  vec3,
  vec4,
  mat4,
  sampler2D,
};
inline std::string_view
ShaderTypeNames(ShaderTypes type)
{
  static const char* s_names[]{
    "vec2", "vec3", "vec4", "mat4", "sampler2D",
  };
  return s_names[(int)type];
}
struct ShaderVariable
{
  ShaderTypes Type;
  std::string Name;
};
inline std::ostream&
operator<<(std::ostream& os, const ShaderVariable& var)
{
  os << ShaderTypeNames(var.Type) << " " << var.Name;
  return os;
}

struct ShaderSnippet
{
  std::vector<ShaderVariable> Inputs;
  std::vector<ShaderVariable> Outputs;
  std::vector<ShaderVariable> Uniforms;
  std::vector<std::string> Codes;
  void In(ShaderTypes type, std::string_view name)
  {
    Inputs.push_back({ type, { name.begin(), name.end() } });
  }
  void Out(ShaderTypes type, std::string_view name)
  {
    Outputs.push_back({ type, { name.begin(), name.end() } });
  }
  void Uniform(ShaderTypes type, std::string_view name)
  {
    Uniforms.push_back({ type, { name.begin(), name.end() } });
  }
  void Code(std::string_view code)
  {
    Codes.push_back({ code.begin(), code.end() });
  }
};

struct VertexAndFragment
{
  ShaderSnippet VS;
  ShaderSnippet FS;
  void Attribute(ShaderTypes type, std::string_view name) { VS.In(type, name); }
  void VsToFs(ShaderTypes type, std::string_view name)
  {
    VS.Out(type, name);
    FS.In(type, name);
  }
  void Out(ShaderTypes type, std::string_view name) { FS.Out(type, name); }
  void Uniform(ShaderTypes type, std::string_view name)
  {
    VS.Uniform(type, name);
    FS.Uniform(type, name);
  }
  void Code(std::string_view code)
  {
    VS.Code(code);
    FS.Code(code);
  }
  void VsEntry(std::string_view code) { VS.Code(code); }
  void FsEntry(std::string_view code) { FS.Code(code); }
};

}

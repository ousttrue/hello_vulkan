#pragma once
#include "../shadersnippet.h"
#include <sstream>

namespace grapho {
namespace gl3 {

/// layout (location=0) in vec3 pos;
/// out VS_OUT{
/// } vs_out;
/// uniform mat4 view;
inline std::string
GenerateVS(const ShaderSnippet& shader,
           std::string_view version = "#version 330 core")
{
  std::stringstream ss;
  ss << version << std::endl << std::endl;

  int i = 0;
  for (auto& var : shader.Inputs) {
    ss << "layout (location = " << (i++) << ") in " << var << ";" << std::endl;
  }
  ss << std::endl;

  ss << "out VS_OUT {" << std::endl;
  for (auto& var : shader.Outputs) {
    ss << "    " << var << ";" << std::endl;
  }
  ss << "} vs_out;" << std::endl << std::endl;

  for (auto& var : shader.Uniforms) {
    ss << "uniform " << var << ";" << std::endl;
  }
  ss << std::endl;

  for (auto& code : shader.Codes) {
    ss << code << std::endl << std::endl;
  }
  return ss.str();
}

/// out vec4 FragColor;
/// in VS_OUT {
/// } fs_in;
/// uniform vec4 color;
inline std::string
GenerateFS(const ShaderSnippet& shader,
           std::string_view version = "#version 330 core")
{
  std::stringstream ss;
  ss << version << std::endl << std::endl;

  for (auto& var : shader.Outputs) {
    ss << "out " << var << ";" << std::endl;
  }

  ss << "in VS_OUT {" << std::endl;
  for (auto& var : shader.Inputs) {
    ss << "    " << var << ";" << std::endl;
  }
  ss << "} fs_in;" << std::endl << std::endl;

  for (auto& var : shader.Uniforms) {
    ss << "uniform " << var << ";" << std::endl;
  }
  ss << std::endl;

  for (auto& code : shader.Codes) {
    ss << code << std::endl << std::endl;
  }
  return ss.str();
}

inline std::string
GenerateVS(const VertexAndFragment& vsfs,
           std::string_view version = "#version 330 core")
{
  return GenerateVS(vsfs.VS, version);
}
inline std::string
GenerateFS(const VertexAndFragment& vsfs,
           std::string_view version = "#version 330 core")
{
  return GenerateFS(vsfs.FS, version);
}

}
}

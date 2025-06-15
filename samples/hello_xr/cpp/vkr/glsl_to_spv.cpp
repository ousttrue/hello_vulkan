#include "glsl_to_spv.h"

#include <iostream>
#include <shaderc/shaderc.hpp>
#include <string>
#include <vector>

// https://developer.android.com/ndk/guides/graphics/shader-compilers?hl=ja
std::vector<uint32_t> compile_file(const std::string &name,
                                   shaderc_shader_kind kind,
                                   const std::string &data) {
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;

  // Like -DMY_DEFINE=1
  options.AddMacroDefinition("MY_DEFINE", "1");

  shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
      data.c_str(), data.size(), kind, name.c_str(), options);

  if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
    std::cerr << module.GetErrorMessage();
  }

  std::vector<uint32_t> result(module.cbegin(), module.cend());
  return result;
}

std::vector<uint32_t> glsl_vs_to_spv(const char *p) {
  return compile_file("vs", shaderc_glsl_vertex_shader, p);
}

std::vector<uint32_t> glsl_fs_to_spv(const char *p) {
  return compile_file("fs", shaderc_glsl_fragment_shader, p);
}

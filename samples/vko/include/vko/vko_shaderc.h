#include "vko.h"

#include <shaderc/shaderc.hpp>
#include <string>
#include <vector>

namespace vko {
// https://developer.android.com/ndk/guides/graphics/shader-compilers?hl=ja
inline std::vector<uint32_t> compile_glsl(const std::string &name,
                                          shaderc_shader_kind kind,
                                          const std::string &data) {

  shaderc::Compiler compiler;
  shaderc::CompileOptions options;

  shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
      data.c_str(), data.size(), kind, name.c_str(), options);

  if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
    Logger::Error("compile_glsl: %s", module.GetErrorMessage().c_str());
  }

  std::vector<uint32_t> result(module.cbegin(), module.cend());
  return result;
}

inline std::vector<uint32_t> glsl_vs_to_spv(const char *p) {
  return compile_glsl("vs", shaderc_glsl_vertex_shader, p);
}

inline std::vector<uint32_t> glsl_fs_to_spv(const char *p) {
  return compile_glsl("fs", shaderc_glsl_fragment_shader, p);
}

} // namespace vko

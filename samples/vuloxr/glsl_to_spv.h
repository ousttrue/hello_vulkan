#pragma once
#include <vector>

std::vector<uint32_t> glsl_vs_to_spv(const char *p);
std::vector<uint32_t> glsl_fs_to_spv(const char *p);

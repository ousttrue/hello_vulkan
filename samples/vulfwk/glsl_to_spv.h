#pragma once
#include <vector>

std::vector<char> glsl_vs_to_spv(const char*p, size_t len);
std::vector<char> glsl_fs_to_spv(const char*p, size_t len);

#pragma once
#include <span>

namespace teapot {

std::span<const float> positions();
std::span<const float> normals();
std::span<const unsigned short> indices();

}

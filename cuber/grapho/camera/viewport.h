#pragma once
#include <array>

namespace grapho {
namespace camera {
struct Viewport
{
  float Left = 0;
  float Top = 0;
  float Width = 0;
  float Height = 0;
  std::array<float, 4> Color = { 1, 0, 1, 0 };
  float Depth = 1.0f;

  float AspectRatio() const { return Width / Height; }
  float Right() const { return Left + Width; }
  float Bottom() const { return Top + Height; }
};

} // namespace
} // namespace

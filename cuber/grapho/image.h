#pragma once
#include "pixelformat.h"
#include <stdint.h>

namespace grapho {

enum class ColorSpace
{
  Linear,
  sRGB,
};

struct Image
{
  int Width;
  int Height;
  PixelFormat Format;
  ColorSpace ColorSpace = ColorSpace::Linear;
  const uint8_t* Pixels = nullptr;
};

}

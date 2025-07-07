#pragma once
#include <DirectXMath.h>
#include <chrono>
#include <ostream>
#include <span>

using BvhOffset = DirectX::XMFLOAT3;

///
/// Mat3 for bvh rotation
///
/// cos sin
///-sin cos
///
///        [0, 1, 2]
/// [x,y,z][3, 4, 5] => [0x + 3y + 6z][1x + 4y + 7z][2x + 5y + 8z]
///        [6, 7, 8]
///
using BvhMat3 = DirectX::XMFLOAT3X3;

enum class BvhChannelTypes {
  None,
  Xposition,
  Yposition,
  Zposition,
  Xrotation,
  Yrotation,
  Zrotation,
};

inline std::string_view to_str(BvhChannelTypes channelType) {
  switch (channelType) {
  case BvhChannelTypes::None:
    return "None";
  case BvhChannelTypes::Xposition:
    return "Xp";
  case BvhChannelTypes::Yposition:
    return "Yp";
  case BvhChannelTypes::Zposition:
    return "Zp";
  case BvhChannelTypes::Xrotation:
    return "Xr";
  case BvhChannelTypes::Yrotation:
    return "Yr";
  case BvhChannelTypes::Zrotation:
    return "Zr";
  default:
    throw std::runtime_error("unknown");
  }
}

struct BvhChannels {
  BvhOffset init;
  size_t startIndex;
  BvhChannelTypes types[6] = {};
  BvhChannelTypes operator[](size_t index) const { return types[index]; }
  BvhChannelTypes &operator[](size_t index) { return types[index]; }
  size_t size() const {
    size_t i = 0;
    for (; i < 6; ++i) {
      if (types[i] == BvhChannelTypes::None) {
        break;
      }
    }
    return i;
  }
};

inline std::ostream &operator<<(std::ostream &os, const BvhChannels channels) {
  for (int i = 0; i < 6; ++i) {
    if (channels[i] == BvhChannelTypes::None) {
      break;
    }
    if (i) {
      os << ", ";
    }
    os << to_str(channels[i]);
  }
  return os;
}

using BvhTime = std::chrono::duration<float, std::ratio<1, 1>>;

struct BvhFrame {
  int index;
  BvhTime time;
  std::span<const float> values;

  std::tuple<BvhOffset, DirectX::XMMATRIX> Resolve(const BvhChannels &channels) const;
};

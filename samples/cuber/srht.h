#pragma once
#include <cmath>
#include <stdint.h>

//
// from
// https://github.com/i-saint/Glimmer/blob/master/Source/MeshUtils/muQuat32.h
//
namespace quat_packer {

static constexpr float SR2 = 1.41421356237f;
static constexpr float RSR2 = 1.0f / 1.41421356237f;
static constexpr float C = float(0x3ff);
static constexpr float R = 1.0f / float(0x3ff);

inline constexpr uint32_t pack(float a) {
  return static_cast<uint32_t>((a * SR2 + 1.0f) * 0.5f * C);
}
inline constexpr float unpack(uint32_t a) {
  return ((a * R) * 2.0f - 1.0f) * RSR2;
}
inline constexpr float square(float a) { return a * a; }
inline int dropmax(float a, float b, float c, float d) {
  if (a > b && a > c && a > d)
    return 0;
  if (b > c && b > d)
    return 1;
  if (c > d)
    return 2;
  return 3;
}
inline float sign(float v) { return v < float(0.0) ? float(-1.0) : float(1.0); }

union Packed {
  uint32_t value;
  struct {
    uint32_t x0 : 10;
    uint32_t x1 : 10;
    uint32_t x2 : 10;
    uint32_t drop : 2;
  };
};

inline uint32_t Pack(float x, float y, float z, float w) {

  Packed value;

  float a0, a1, a2;
  value.drop = dropmax(square(x), square(y), square(z), square(w));
  if (value.drop == 0) {
    float s = sign(x);
    a0 = y * s;
    a1 = z * s;
    a2 = w * s;
  } else if (value.drop == 1) {
    float s = sign(y);
    a0 = x * s;
    a1 = z * s;
    a2 = w * s;
  } else if (value.drop == 2) {
    float s = sign(z);
    a0 = x * s;
    a1 = y * s;
    a2 = w * s;
  } else {
    float s = sign(w);
    a0 = x * s;
    a1 = y * s;
    a2 = z * s;
  }

  value.x0 = pack(a0);
  value.x1 = pack(a1);
  value.x2 = pack(a2);

  return *(uint32_t *)&value;
}

inline void Unpack(uint32_t src, float values[4]) {

  auto value = (Packed *)&src;

  const float a0 = unpack(value->x0);
  const float a1 = unpack(value->x1);
  const float a2 = unpack(value->x2);
  const float iss = std::sqrt(1.0f - (square(a0) + square(a1) + square(a2)));

  switch (value->drop) {
  case 0: {
    values[0] = iss;
    values[1] = a0;
    values[2] = a1;
    values[3] = a2;
    break;
  }
  case 1: {
    values[0] = a0;
    values[1] = iss;
    values[2] = a1;
    values[3] = a2;
    break;
  }
  case 2: {
    values[0] = a0;
    values[1] = a1;
    values[2] = iss;
    values[3] = a2;
    break;
  }
  default: {
    values[0] = a0;
    values[1] = a1;
    values[2] = a2;
    values[3] = iss;
    break;
  }
  }
}

} // namespace quat_packer

//
// SingleRootHierarchicalTransformation
//
namespace srht {

enum class HumanoidBones : uint16_t {
  UNKNOWN = 0,
  // body: 6
  HIPS = 1,
  SPINE,
  CHEST,
  UPPERCHEST,
  NECK,
  HEAD,
  // legs: 4 x 2
  LEFT_UPPERLEG,
  LEFT_LOWERLEG,
  LEFT_FOOT,
  LEFT_TOES,
  RIGHT_UPPERLEG,
  RIGHT_LOWERLEG,
  RIGHT_FOOT,
  RIGHT_TOES,
  // arms: 4 x 2
  LEFT_SHOULDER,
  LEFT_UPPERARM,
  LEFT_LOWERARM,
  LEFT_HAND,
  RIGHT_SHOULDER,
  RIGHT_UPPERARM,
  RIGHT_LOWERARM,
  RIGHT_HAND,
  // fingers: 3 x 5 x 2
  LEFT_THUMB_METACARPAL,
  LEFT_THUMB_PROXIMAL,
  LEFT_THUMB_DISTAL,
  LEFT_INDEX_PROXIMAL,
  LEFT_INDEX_INTERMEDIATE,
  LEFT_INDEX_DISTAL,
  LEFT_MIDDLE_PROXIMAL,
  LEFT_MIDDLE_INTERMEDIATE,
  LEFT_MIDDLE_DISTAL,
  LEFT_RING_PROXIMAL,
  LEFT_RING_INTERMEDIATE,
  LEFT_RING_DISTAL,
  LEFT_LITTLE_PROXIMAL,
  LEFT_LITTLE_INTERMEDIATE,
  LEFT_LITTLE_DISTAL,
  RIGHT_THUMB_METACARPAL,
  RIGHT_THUMB_PROXIMAL,
  RIGHT_THUMB_DISTAL,
  RIGHT_INDEX_PROXIMAL,
  RIGHT_INDEX_INTERMEDIATE,
  RIGHT_INDEX_DISTAL,
  RIGHT_MIDDLE_PROXIMAL,
  RIGHT_MIDDLE_INTERMEDIATE,
  RIGHT_MIDDLE_DISTAL,
  RIGHT_RING_PROXIMAL,
  RIGHT_RING_INTERMEDIATE,
  RIGHT_RING_DISTAL,
  RIGHT_LITTLE_PROXIMAL,
  RIGHT_LITTLE_INTERMEDIATE,
  RIGHT_LITTLE_DISTAL,
};
static const char *HumanoidBoneNames[] = {
    "UNKNOWN",
    // body: 6
    "HIPS",
    "SPINE",
    "CHEST",
    "UPPERCHEST",
    "NECK",
    "HEAD",
    // legs: 4 x 2
    "LEFT_UPPERLEG",
    "LEFT_LOWERLEG",
    "LEFT_FOOT",
    "LEFT_TOES",
    "RIGHT_UPPERLEG",
    "RIGHT_LOWERLEG",
    "RIGHT_FOOT",
    "RIGHT_TOES",
    // arms: 4 x 2
    "LEFT_SHOULDER",
    "LEFT_UPPERARM",
    "LEFT_LOWERARM",
    "LEFT_HAND",
    "RIGHT_SHOULDER",
    "RIGHT_UPPERARM",
    "RIGHT_LOWERARM",
    "RIGHT_HAND",
    // fingers: 3 x 5 x 2
    "LEFT_THUMB_METACARPAL",
    "LEFT_THUMB_PROXIMAL",
    "LEFT_THUMB_DISTAL",
    "LEFT_INDEX_PROXIMAL",
    "LEFT_INDEX_INTERMEDIATE",
    "LEFT_INDEX_DISTAL",
    "LEFT_MIDDLE_PROXIMAL",
    "LEFT_MIDDLE_INTERMEDIATE",
    "LEFT_MIDDLE_DISTAL",
    "LEFT_RING_PROXIMAL",
    "LEFT_RING_INTERMEDIATE",
    "LEFT_RING_DISTAL",
    "LEFT_LITTLE_PROXIMAL",
    "LEFT_LITTLE_INTERMEDIATE",
    "LEFT_LITTLE_DISTAL",
    "RIGHT_THUMB_METACARPAL",
    "RIGHT_THUMB_PROXIMAL",
    "RIGHT_THUMB_DISTAL",
    "RIGHT_INDEX_PROXIMAL",
    "RIGHT_INDEX_INTERMEDIATE",
    "RIGHT_INDEX_DISTAL",
    "RIGHT_MIDDLE_PROXIMAL",
    "RIGHT_MIDDLE_INTERMEDIATE",
    "RIGHT_MIDDLE_DISTAL",
    "RIGHT_RING_PROXIMAL",
    "RIGHT_RING_INTERMEDIATE",
    "RIGHT_RING_DISTAL",
    "RIGHT_LITTLE_PROXIMAL",
    "RIGHT_LITTLE_INTERMEDIATE",
    "RIGHT_LITTLE_DISTAL",
};

union PackQuat {
  uint32_t value;
};

struct JointDefinition {
  // parentBone(-1 for root)
  uint16_t parentBoneIndex;
  // HumanBones or any number
  uint16_t boneType;
  float xFromParent;
  float yFromParent;
  float zFromParent;
};
static_assert(sizeof(JointDefinition) == 16, "JointDefintion");

enum class SkeletonFlags : uint32_t {
  NONE = 0,
  // if hasInitialRotation PackQuat X jointCount for InitialRotation
  HAS_INITIAL_ROTATION = 0x1,
};

struct SkeletonHeader {
  char magic[8] = {'S', 'R', 'H', 'T', 'S', 'K', 'L', '1'};
  uint16_t skeletonId = 0;
  uint16_t jointCount = 0;
  SkeletonFlags flags = {};
};
// continue JointDefinition X jointCount
static_assert(sizeof(SkeletonHeader) == 16, "Skeleton");

enum class FrameFlags : uint32_t {
  NONE = 0,
  // enableed rotation is Quat32: disabled rotation is float4(x, y, z, w)
  USE_QUAT32 = 0x1,
};

struct FrameHeader {
  char magic[8] = {'S', 'R', 'H', 'T', 'F', 'R', 'M', '1'};
  // std::chrono::nanoseconds
  int64_t time;
  FrameFlags flags = {};
  uint16_t skeletonId = 0;
  // root position
  float x;
  float y;
  float z;
};
// continue PackQuat x SkeletonHeader::JointCount
static_assert(sizeof(FrameHeader) == 40, "FrameSize");

} // namespace srht

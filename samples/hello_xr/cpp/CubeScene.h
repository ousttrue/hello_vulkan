#pragma once
#include <openxr/openxr.h>
#include <span>
#include <vector>
#include <vuloxr.h>

struct Cube {
  XrPosef pose;
  XrVector3f scale;
};

struct Mat4 {
  float m[16];
};

struct Vec3 {
  float x, y, z;
};

struct Vertex {
  Vec3 Position;
  Vec3 Color;
};

constexpr Vec3 Red{1, 0, 0};
constexpr Vec3 DarkRed{0.25f, 0, 0};
constexpr Vec3 Green{0, 1, 0};
constexpr Vec3 DarkGreen{0, 0.25f, 0};
constexpr Vec3 Blue{0, 0, 1};
constexpr Vec3 DarkBlue{0, 0, 0.25f};

// Vertices for a 1x1x1 meter cube. (Left/Right, Top/Bottom, Front/Back)
constexpr Vec3 LBB{-0.5f, -0.5f, -0.5f};
constexpr Vec3 LBF{-0.5f, -0.5f, 0.5f};
constexpr Vec3 LTB{-0.5f, 0.5f, -0.5f};
constexpr Vec3 LTF{-0.5f, 0.5f, 0.5f};
constexpr Vec3 RBB{0.5f, -0.5f, -0.5f};
constexpr Vec3 RBF{0.5f, -0.5f, 0.5f};
constexpr Vec3 RTB{0.5f, 0.5f, -0.5f};
constexpr Vec3 RTF{0.5f, 0.5f, 0.5f};

#define CUBE_SIDE(V1, V2, V3, V4, V5, V6, COLOR)                               \
  {V1, COLOR}, {V2, COLOR}, {V3, COLOR}, {V4, COLOR}, {V5, COLOR}, {V6, COLOR},

constexpr Vertex c_cubeVertices[] = {
    CUBE_SIDE(LTB, LBF, LBB, LTB, LTF, LBF, DarkRed)   // -X
    CUBE_SIDE(RTB, RBB, RBF, RTB, RBF, RTF, Red)       // +X
    CUBE_SIDE(LBB, LBF, RBF, LBB, RBF, RBB, DarkGreen) // -Y
    CUBE_SIDE(LTB, RTB, RTF, LTB, RTF, LTF, Green)     // +Y
    CUBE_SIDE(LBB, RBB, RTB, LBB, RTB, LTB, DarkBlue)  // -Z
    CUBE_SIDE(LBF, LTF, RTF, LBF, RTF, RBF, Blue)      // +Z
};

// Winding order is clockwise. Each side uses a different color.
constexpr unsigned short c_cubeIndices[] = {
    0,  1,  2,  3,  4,  5,  // -X
    6,  7,  8,  9,  10, 11, // +X
    12, 13, 14, 15, 16, 17, // -Y
    18, 19, 20, 21, 22, 23, // +Y
    24, 25, 26, 27, 28, 29, // -Z
    30, 31, 32, 33, 34, 35, // +Z
};

struct CubeScene : vuloxr::NonCopyable {
  std::vector<XrSpace> spaces;
  std::vector<Cube> cubes;
  std::vector<Mat4> matrices;

  CubeScene(XrSession session);
  ~CubeScene();
  void addSpace(XrSession session, XrReferenceSpaceType spaceType,
                const XrPosef &pose = {.orientation = {0, 0, 0, 1.0f},
                                       .position = {0, 0, 0}});
  void beginFrame();
  std::span<const Mat4> endFrame(XrPosef hmdPose, XrFovf fov);
};

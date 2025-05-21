#pragma once

struct Vec3 {
  float x, y, z;
};

struct Vec4 {
  float x, y, z, w;
};

struct Mat4 {
  float m[16];
};

struct Cube {
  Vec3 Translaton;
  Vec4 Rotation;
  Vec3 Scaling;
};

struct Vertex {
  Vec3 Position;
  Vec3 Color;
};

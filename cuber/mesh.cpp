#include <cuber/mesh.h>

using namespace grapho;

namespace cuber {

static VertexLayout layouts[] = {
    {
        .Id =
            {
                .AttributeLocation = 0,
                .Slot= 0,
                .SemanticName = "POSITION",
                .SemanticIndex = 0,
            },
        .Type = ValueType::Float,
        .Count = 4,
        .Offset = offsetof(Vertex, PositionFace),
        .Stride = sizeof(Vertex),
    },
    {
        .Id =
            {
                .AttributeLocation = 1,
                .Slot = 0,
                .SemanticName = "TEXCOORD",
                .SemanticIndex = 0,
            },
        .Type = ValueType::Float,
        .Count = 4,
        .Offset = offsetof(Vertex, UvBarycentric),
        .Stride = sizeof(Vertex),
    },
    //
    {
        .Id =
            {
                .AttributeLocation = 2,
                .Slot = 1,
                .SemanticName = "ROW",
                .SemanticIndex = 0,
            },
        .Type = ValueType::Float,
        .Count = 4,
        .Offset = offsetof(Instance, Row0),
        .Stride = sizeof(Instance),
        .Divisor = 1,
    },
    {
        .Id =
            {
                .AttributeLocation = 3,
                .Slot = 1,
                .SemanticName = "ROW",
                .SemanticIndex = 1,
            },
        .Type = ValueType::Float,
        .Count = 4,
        .Offset = offsetof(Instance, Row1),
        .Stride = sizeof(Instance),
        .Divisor = 1,
    },
    {
        .Id =
            {
                .AttributeLocation = 4,
                .Slot = 1,
                .SemanticName = "ROW",
                .SemanticIndex = 2,
            },
        .Type = ValueType::Float,
        .Count = 4,
        .Offset = offsetof(Instance, Row2),
        .Stride = sizeof(Instance),
        .Divisor = 1,
    },
    {
        .Id =
            {
                .AttributeLocation = 5,
                .Slot = 1,
                .SemanticName = "ROW",
                .SemanticIndex = 3,
            },
        .Type = ValueType::Float,
        .Count = 4,
        .Offset = offsetof(Instance, Row3),
        .Stride = sizeof(Instance),
        .Divisor = 1,
    },
    //
    {
        .Id =
            {
                .AttributeLocation = 6,
                .Slot = 1,
                .SemanticName = "FACE",
                .SemanticIndex = 0,
            },
        .Type = ValueType::Float,
        .Count = 4,
        .Offset = offsetof(Instance, PositiveFaceFlag),
        .Stride = sizeof(Instance),
        .Divisor = 1,
    },
    {
        .Id =
            {
                .AttributeLocation = 7,
                .Slot = 1,
                .SemanticName = "FACE",
                .SemanticIndex = 1,
            },
        .Type = ValueType::Float,
        .Count = 4,
        .Offset = offsetof(Instance, NegativeFaceFlag),
        .Stride = sizeof(Instance),
        .Divisor = 1,
    },
};

const float s = 0.5f;
DirectX::XMFLOAT3 positions[8] = {
  { +s, -s, -s }, //
  { +s, -s, +s }, //
  { +s, +s, +s }, //
  { +s, +s, -s }, //
  { -s, -s, -s }, //
  { -s, -s, +s }, //
  { -s, +s, +s }, //
  { -s, +s, -s }, //
};

//   7+-+3
//   / /|
// 6+-+2|
//  |4+-+0
//  |/ /
// 5+-+1
//
//   Y
//   A
//   +-> X
//  /
// L
//
struct Face
{
  int Indices[4];
  DirectX::XMFLOAT2 Uv[4];
};

// CCW
Face cube_faces[6] = {
  // x+
  {
    .Indices = { 2, 1, 0, 3 },
    .Uv = { { 1, 0 }, { 1, 1 }, { 2, 1 }, { 2, 0 } },
  },
  // y+
  {
    .Indices = { 2, 3, 7, 6 },
    .Uv = { { 1, 0 }, { 1, -1 }, { 0, -1 }, { 0, 0 } },
  },
  // z+
  {
    .Indices = { 2, 6, 5, 1 },
    .Uv = { { 1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 } },
  },
  // x-
  {
    .Indices = { 4, 5, 6, 7 },
    .Uv = { { -1, 1 }, { 0, 1 }, { 0, 0 }, { -1, 0 } },
  },
  // y-
  {
    .Indices = { 4, 0, 1, 5 },
    .Uv = { { 0, 2 }, { 1, 2 }, { 1, 1 }, { 0, 1 } },
  },
  // z-
  {
    .Indices = { 4, 7, 3, 0 },
    .Uv = { { 0, 1 }, { 0, 0 }, { 1, 0 }, { 1, 1 } },
  },
};

struct Builder
{
  Mesh Mesh;
  bool CCW;
  Builder(bool isCCW)
    : CCW(isCCW)
  {
  }

  void Quad(int face,
            const DirectX::XMFLOAT3& p0,
            const DirectX::XMFLOAT2& uv0,
            const DirectX::XMFLOAT3& p1,
            const DirectX::XMFLOAT2& uv1,
            const DirectX::XMFLOAT3& p2,
            const DirectX::XMFLOAT2& uv2,
            const DirectX::XMFLOAT3& p3,
            const DirectX::XMFLOAT2& uv3)
  {
    // 01   00
    //  3+-+2
    //   | |
    //  0+-+1
    // 00   10
    Vertex v0{
      .PositionFace = { p0.x, p0.y, p0.z, (float)face },
      .UvBarycentric = { uv0.x, uv0.y, 1, 0 },
    };
    Vertex v1{
      .PositionFace = { p1.x, p1.y, p1.z, (float)face },
      .UvBarycentric = { uv1.x, uv1.y, 0, 0 },
    };
    Vertex v2{
      .PositionFace = { p2.x, p2.y, p2.z, (float)face },
      .UvBarycentric = { uv2.x, uv2.y, 0, 1 },
    };
    Vertex v3{
      .PositionFace = { p3.x, p3.y, p3.z, (float)face },
      .UvBarycentric = { uv3.x, uv3.y, 0, 0 },
    };
    auto index = Mesh.Vertices.size();
    Mesh.Vertices.push_back(v0);
    Mesh.Vertices.push_back(v1);
    Mesh.Vertices.push_back(v2);
    Mesh.Vertices.push_back(v3);
    if (CCW) {
      // 0, 1, 2
      Mesh.Indices.push_back(index);
      Mesh.Indices.push_back(index + 1);
      Mesh.Indices.push_back(index + 2);
      // 2, 3, 0
      Mesh.Indices.push_back(index + 2);
      Mesh.Indices.push_back(index + 3);
      Mesh.Indices.push_back(index);
    } else {
      // 0, 3, 2
      Mesh.Indices.push_back(index);
      Mesh.Indices.push_back(index + 3);
      Mesh.Indices.push_back(index + 2);
      // 2, 1, 0
      Mesh.Indices.push_back(index + 2);
      Mesh.Indices.push_back(index + 1);
      Mesh.Indices.push_back(index);
    }
  }
};

Mesh
Cube(bool isCCW, bool isStereo)
{
  Builder builder(isCCW);
  for (auto layout : layouts) {
    layout.Divisor *= (isStereo ? 2 : 1);
    builder.Mesh.Layouts.push_back(layout);
  }
  int f = 0;
  for (auto face : cube_faces) {
    builder.Quad(f++,
                 positions[face.Indices[0]],
                 face.Uv[0],
                 positions[face.Indices[1]],
                 face.Uv[1],
                 positions[face.Indices[2]],
                 face.Uv[2],
                 positions[face.Indices[3]],
                 face.Uv[3]);
  }
  return builder.Mesh;
}

void
PushGrid(std::vector<LineVertex>& lines, float interval, int half_count)
{
  const float half = interval * half_count;
  for (int i = -half_count; i <= half_count; ++i) {
    if (i) {
      lines.push_back({
        .Position = { -half, 0, static_cast<float>(i) },
        .Color = Pallete::White,
      });
      lines.push_back({
        .Position = { half, 0, static_cast<float>(i) },
        .Color = Pallete::White,
      });
      lines.push_back({
        .Position = { static_cast<float>(i), 0, -half },
        .Color = Pallete::White,
      });
      lines.push_back({
        .Position = { static_cast<float>(i), 0, half },
        .Color = Pallete::White,
      });
    }
  }

  lines.push_back({
    .Position = { half, 0, 0 },
    .Color = Pallete::Red,
  });
  lines.push_back({
    .Position = { 0, 0, 0 },
    .Color = Pallete::Red,
  });
  lines.push_back({
    .Position = { -half, 0, 0 },
    .Color = Pallete::DarkRed,
  });
  lines.push_back({
    .Position = { 0, 0, 0 },
    .Color = Pallete::DarkRed,
  });

  lines.push_back({
    .Position = { 0, 0, half },
    .Color = Pallete::Blue,
  });
  lines.push_back({
    .Position = { 0, 0, 0 },
    .Color = Pallete::Blue,
  });
  lines.push_back({
    .Position = { 0, 0, -half },
    .Color = Pallete::DarkBlue,
  });
  lines.push_back({
    .Position = { 0, 0, 0 },
    .Color = Pallete::DarkBlue,
  });
}

} // namespace cuber

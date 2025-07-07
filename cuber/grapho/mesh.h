#pragma once
#include "vertexlayout.h"
#include <memory>
#include <vector>
#include <cmath>

namespace grapho {
namespace mesh {

struct Vertex
{
  DirectX::XMFLOAT3 Position;
  DirectX::XMFLOAT3 Normal;
  DirectX::XMFLOAT2 Uv;

  static VertexLayout Layouts[3];
};

inline VertexLayout Vertex::Layouts[3] = {
  {
    .Id = {
     .AttributeLocation=0,
     .Slot=0,
    },
    .Type = grapho::ValueType::Float,
    .Count = 3,
    .Offset = offsetof(Vertex, Position),
    .Stride = sizeof(Vertex),
  },
  {
    .Id = {
     .AttributeLocation=1,
     .Slot=0,
    },
    .Type = grapho::ValueType::Float,
    .Count = 3,
    .Offset = offsetof(Vertex, Normal),
    .Stride = sizeof(Vertex),
  },
  {
    .Id = {
     .AttributeLocation=2,
     .Slot=0,
    },
    .Type = grapho::ValueType::Float,
    .Count = 2,
    .Offset = offsetof(Vertex, Uv),
    .Stride = sizeof(Vertex),
  },
};

inline std::shared_ptr<Mesh>
Sphere()
{
  std::vector<DirectX::XMFLOAT3> positions;
  std::vector<DirectX::XMFLOAT2> uv;
  std::vector<DirectX::XMFLOAT3> normals;
  const unsigned int X_SEGMENTS = 64;
  const unsigned int Y_SEGMENTS = 64;
  const float PI = 3.14159265359f;
  for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
    for (unsigned int y = 0; y <= Y_SEGMENTS; ++y) {
      float xSegment = (float)x / (float)X_SEGMENTS;
      float ySegment = (float)y / (float)Y_SEGMENTS;
      float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
      float yPos = std::cos(ySegment * PI);
      float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
      positions.push_back({ xPos, yPos, zPos });
      uv.push_back({ xSegment, ySegment });
      normals.push_back({ xPos, yPos, zPos });
    }
  }

  bool oddRow = false;
  std::vector<uint16_t> indices;
  for (unsigned int y = 0; y < Y_SEGMENTS; ++y) {
    if (!oddRow) // even rows: y == 0, y == 2; and so on
    {
      for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
        indices.push_back(y * (X_SEGMENTS + 1) + x);
        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
      }
    } else {
      for (int x = X_SEGMENTS; x >= 0; --x) {
        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
        indices.push_back(y * (X_SEGMENTS + 1) + x);
      }
    }
    oddRow = !oddRow;
  }

  std::vector<Vertex> vertices;
  for (unsigned int i = 0; i < positions.size(); ++i) {
    vertices.push_back({
      positions[i],
      normals[i],
      uv[i],
    });
  }

  auto ptr = std::make_shared<Mesh>();
  ptr->Layouts.assign(Vertex::Layouts,
                      Vertex::Layouts + std::size(Vertex::Layouts));
  ptr->Vertices.Assign(vertices);
  ptr->Indices.Assign(indices);
  ptr->Mode = DrawMode::TriangleStrip;
  return ptr;
}

inline std::shared_ptr<Mesh>
Cube(float s = 1.0f)
{
  std::vector<Vertex> vertices{
    // back face
    { { -s, -s, -s }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } }, // bottom-left
    { { s, s, -s }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f } },   // top-right
    { { s, -s, -s }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f } },  // bottom-right
    { { s, s, -s }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f } },   // top-right
    { { -s, -s, -s }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } }, // bottom-left
    { { -s, s, -s }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f } },  // top-left
    // front face
    { { -s, -s, s }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } }, // bottom-left
    { { s, -s, s }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },  // bottom-right
    { { s, s, s }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },   // top-right
    { { s, s, s }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },   // top-right
    { { -s, s, s }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },  // top-left
    { { -s, -s, s }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } }, // bottom-left
    // left face
    { { -s, s, s }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },   // top-right
    { { -s, s, -s }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },  // top-left
    { { -s, -s, -s }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } }, // bottom-left
    { { -s, -s, -s }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } }, // bottom-left
    { { -s, -s, s }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },  // bottom-right
    { { -s, s, s }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },   // top-right
                                                               // right face
    { { s, s, s }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },     // top-left
    { { s, -s, -s }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },   // bottom-right
    { { s, s, -s }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },    // top-right
    { { s, -s, -s }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },   // bottom-right
    { { s, s, s }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },     // top-left
    { { s, -s, s }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },    // bottom-left
    // bottom face
    { { -s, -s, -s }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } }, // top-right
    { { s, -s, -s }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },  // top-left
    { { s, -s, s }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } },   // bottom-left
    { { s, -s, s }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } },   // bottom-left
    { { -s, -s, s }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },  // bottom-right
    { { -s, -s, -s }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } }, // top-right
    // top face
    { { -s, s, -s }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } }, // top-left
    { { s, s, s }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },   // bottom-right
    { { s, s, -s }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },  // top-right
    { { s, s, s }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },   // bottom-right
    { { -s, s, -s }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } }, // top-left
    { { -s, s, s }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } }   // bottom-left
  };
  auto ptr = std::make_shared<Mesh>();
  ptr->Layouts.assign(Vertex::Layouts,
                      Vertex::Layouts + std::size(Vertex::Layouts));
  ptr->Vertices.Assign(vertices);
  ptr->Mode = DrawMode::Triangles;
  return ptr;
}

struct QuadVertex
{
  DirectX::XMFLOAT3 Position;
  DirectX::XMFLOAT2 Uv;

  static VertexLayout Layouts[2];
};

inline VertexLayout QuadVertex::Layouts[2] = {
  {
    .Id = {
     .AttributeLocation=0,
     .Slot=0,
    },
    .Type = grapho::ValueType::Float,
    .Count = 3,
    .Offset = offsetof(Vertex, Position),
    .Stride = sizeof(Vertex),
  },
  {
    .Id = {
     .AttributeLocation=1,
     .Slot=0,
    },
    .Type = grapho::ValueType::Float,
    .Count = 2,
    .Offset = offsetof(Vertex, Uv),
    .Stride = sizeof(Vertex),
  },
};

inline std::shared_ptr<Mesh>
Quad()
{
  QuadVertex vertices[4] = {
    // positions        // texture Coords
    { { -1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
    { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },
    { { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
    { { 1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } },
  };

  auto ptr = std::make_shared<Mesh>();
  ptr->Layouts.assign(QuadVertex::Layouts,
                      QuadVertex::Layouts + std::size(QuadVertex::Layouts));
  ptr->Vertices.Assign(vertices);
  ptr->Mode = DrawMode::TriangleStrip;
  return ptr;
}

}
}

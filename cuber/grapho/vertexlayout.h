#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>

namespace grapho {

enum class ValueType
{
  Float,
  Double,
  Int8,
  Int16,
  Int32,
  UInt8,
  UInt16,
  UInt32,
};

struct VertexId
{
  uint32_t AttributeLocation;
  uint32_t Slot;
  std::string SemanticName;
  uint32_t SemanticIndex;
};

struct VertexLayout
{
  VertexId Id;
  ValueType Type;
  uint32_t Count;
  uint32_t Offset;
  uint32_t Stride;
  uint32_t Divisor = 0;
};

enum class DrawMode
{
  Triangles,
  TriangleStrip,
};

struct VertexBuffer
{
  std::vector<uint8_t> Bytes;
  uint32_t Count = 0;
  template<typename T>
  void Assign(const std::vector<T>& values)
  {
    Count = values.size();
    Bytes.assign((const uint8_t*)values.data(),
                 (const uint8_t*)(values.data() + Count));
  }
  template<typename T, size_t N>
  void Assign(const T (&values)[N])
  {
    Count = N;
    Bytes.assign((const uint8_t*)values, (const uint8_t*)(values + Count));
  }
  uint32_t Size() const { return Bytes.size(); }
  const uint8_t* Data() const { return Bytes.data(); }
  uint32_t Stride() const { return Bytes.size() / Count; }
};

struct Mesh
{
  DrawMode Mode = DrawMode::Triangles;
  std::vector<grapho::VertexLayout> Layouts;
  VertexBuffer Vertices;
  VertexBuffer Indices;
  uint32_t DrawCount() const
  {
    return Indices.Size() ? Indices.Count : Vertices.Count;
  }
};

template<typename T>
struct SpanModoki
{
  T* ptr;
  size_t count;
  size_t size() const { return count; }
  T& operator[](size_t i) { return ptr[i]; }
  const T& operator[](size_t i) const { return ptr[i]; }
  T* begin() { return ptr; }
  T* end() { return ptr + count; }
  const T* begin() const { return ptr; }
  const T* end() const { return ptr + count; }
};

template<typename S, size_t N>
inline SpanModoki<S>
make_span(const S (&values)[N])
{
  return SpanModoki<S>{
    (S*)values,
    N,
  };
}

template<typename S, size_t N>
inline SpanModoki<S>
make_span(const std::array<S, N>& values)
{
  return SpanModoki<S>{
    values.data(),
    values.size(),
  };
}

template<typename S>
inline SpanModoki<S>
make_span(const std::vector<S>& values)
{
  return SpanModoki<S>{
    (S*)values.data(),
    values.size(),
  };
}

}

#pragma once
#include "../vertexlayout.h"
#include <assert.h>
#include <memory>
#include <optional>
#include <stdint.h>
#include <vector>

namespace grapho {
namespace gl3 {

std::optional<uint32_t>
GLType(ValueType type);

std::optional<uint32_t>
GLIndexTypeFromStride(uint32_t stride);

std::optional<uint32_t>
GLMode(grapho::DrawMode mode);

class Vbo
{
  uint32_t vbo_ = 0;

  Vbo(uint32_t vbo);

public:
  ~Vbo();
  static std::shared_ptr<Vbo> Create(uint32_t size, const void* data);

  template<typename T>
  static std::shared_ptr<Vbo> Create(const T& array)
  {
    return Create(sizeof(array), array);
  }

  template<typename T>
  static std::shared_ptr<Vbo> Create(const std::vector<T>& values)
  {
    return Create(sizeof(T) * values.size(), values.data());
  }

  void Bind();
  void Unbind();
  void Upload(uint32_t size, const void* data);
};

class Ibo
{
  uint32_t ibo_ = 0;

  Ibo(uint32_t ibo, uint32_t valuetype);

public:
  uint32_t valuetype_ = 0;
  ~Ibo();
  static std::shared_ptr<Ibo> Create(uint32_t size,
                                     const void* data,
                                     uint32_t valuetype);
  template<typename T>
  static std::shared_ptr<Ibo> Create(const std::vector<T>& values)
  {
    switch (sizeof(T)) {
      case 1:
        return Create(
          values.size(), values.data(), 0x1401 /*GL_UNSIGNED_BYTE*/);

      case 2:
        return Create(
          values.size() * 2, values.data(), 0x1403 /*GL_UNSIGNED_SHORT*/);

      case 4:
        return Create(
          values.size() * 4, values.data(), 0x1405 /*GL_UNSIGNED_INT*/);
    }
    return {};
  }
  void Bind();
  void Unbind();
};

struct Vao
{
  uint32_t vao_ = 0;
  std::vector<VertexLayout> layouts_;
  std::vector<std::shared_ptr<Vbo>> slots_;
  std::shared_ptr<Ibo> ibo_;

  Vao(uint32_t vao,
      SpanModoki<VertexLayout> layouts,
      SpanModoki<std::shared_ptr<Vbo>> slots,
      const std::shared_ptr<Ibo>& ibo);

  ~Vao();
  static std::shared_ptr<Vao> Create(SpanModoki<VertexLayout> layouts,
                                     SpanModoki<std::shared_ptr<Vbo>> slots,
                                     const std::shared_ptr<Ibo>& ibo = {});
  static std::shared_ptr<Vao> Create(const std::shared_ptr<Mesh>& mesh);
  void Bind();
  void Unbind();
  void Draw(uint32_t mode, uint32_t count, uint32_t offsetBytes = 0);
  void DrawInstance(uint32_t primcount,
                    uint32_t count,
                    uint32_t offsetBytes = 0);
};

} // namespace
} // namespace

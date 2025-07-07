#include <GL/glew.h>
#include <stdexcept>

#include "vao.h"

namespace grapho {
namespace gl3 {

std::optional<uint32_t>
GLType(ValueType type)
{
  switch (type) {
    case ValueType::Float:
      return GL_FLOAT;
    case ValueType::Double:
      return GL_DOUBLE;

    case ValueType::Int8:
      return GL_BYTE;
    case ValueType::Int16:
      return GL_SHORT;
    case ValueType::Int32:
      return GL_INT;

    case ValueType::UInt8:
      return GL_UNSIGNED_BYTE;
    case ValueType::UInt16:
      return GL_UNSIGNED_SHORT;
    case ValueType::UInt32:
      return GL_UNSIGNED_INT;
  }

  return {};
}

std::optional<uint32_t>
GLIndexTypeFromStride(uint32_t stride)
{
  switch (stride) {
    case 1:
      return GL_UNSIGNED_BYTE;

    case 2:
      return GL_UNSIGNED_SHORT;

    case 4:
      return GL_UNSIGNED_INT;

    default:
      // return std::unexpected{ "invalid index stride" };
      return {};
  }
}

std::optional<uint32_t>
GLMode(grapho::DrawMode mode)
{
  switch (mode) {
    case DrawMode::Triangles:
      return GL_TRIANGLES;
    case DrawMode::TriangleStrip:
      return GL_TRIANGLE_STRIP;
    default:
      // return std::unexpected{ "unknown GLType" };
      return {};
  }
}

Vbo::Vbo(uint32_t vbo)
  : vbo_(vbo)
{
}

Vbo::~Vbo()
{
  glDeleteBuffers(1, &vbo_);
}
std::shared_ptr<Vbo>
Vbo::Create(uint32_t size, const void* data)
{
  GLuint vbo;
  glGenBuffers(1, &vbo);
  auto ptr = std::shared_ptr<Vbo>(new Vbo(vbo));
  ptr->Bind();
  if (data) {
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
  } else {
    glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
  }
  ptr->Unbind();
  return ptr;
}

void
Vbo::Bind()
{
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
}
void
Vbo::Unbind()
{
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}
void
Vbo::Upload(uint32_t size, const void* data)
{
  Bind();
  glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
  Unbind();
}

Ibo::Ibo(uint32_t ibo, uint32_t valuetype)
  : ibo_(ibo)
  , valuetype_(valuetype)
{
}
Ibo::~Ibo()
{
  glDeleteBuffers(1, &ibo_);
}
std::shared_ptr<Ibo>
Ibo::Create(uint32_t size, const void* data, uint32_t valuetype)
{
  GLuint ibo;
  glGenBuffers(1, &ibo);
  auto ptr = std::shared_ptr<Ibo>(new Ibo(ibo, valuetype));
  ptr->Bind();
  if (data) {
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
  } else {
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
  }
  ptr->Unbind();
  return ptr;
}

void
Ibo::Bind()
{
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
}
void
Ibo::Unbind()
{
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

Vao::Vao(uint32_t vao,
         SpanModoki<VertexLayout> layouts,
         SpanModoki<std::shared_ptr<Vbo>> slots,
         const std::shared_ptr<Ibo>& ibo)
  : vao_(vao)
  , layouts_(layouts.begin(), layouts.end())
  , slots_(slots.begin(), slots.end())
  , ibo_(ibo)
{
  Bind();
  if (ibo_) {
    ibo_->Bind();
  }
  for (int i = 0; i < layouts.size(); ++i) {
    auto& layout = layouts[i];
    glEnableVertexAttribArray(layout.Id.AttributeLocation);
    slots[layout.Id.Slot]->Bind();
    glVertexAttribPointer(
      layout.Id.AttributeLocation,
      layout.Count,
      *GLType(layout.Type),
      GL_FALSE,
      layout.Stride,
      reinterpret_cast<void*>(static_cast<uint64_t>(layout.Offset)));
    if (layout.Divisor) {
      // auto a = glVertexAttribDivisor;
      glVertexAttribDivisor(layout.Id.AttributeLocation, layout.Divisor);
    }
  }
  Unbind();
  if (ibo_) {
    ibo_->Unbind();
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

Vao::~Vao()
{
  glDeleteVertexArrays(1, &vao_);
}

std::shared_ptr<Vao>
Vao::Create(SpanModoki<VertexLayout> layouts,
            SpanModoki<std::shared_ptr<Vbo>> slots,
            const std::shared_ptr<Ibo>& ibo)
{
  GLuint vao;
  glGenVertexArrays(1, &vao);
  auto ptr = std::shared_ptr<Vao>(new Vao(vao, layouts, slots, ibo));
  return ptr;
}

std::shared_ptr<Vao>
Vao::Create(const std::shared_ptr<Mesh>& mesh)
{
  std::shared_ptr<Vbo> slots[1] = { Vbo::Create(mesh->Vertices.Size(),
                                                mesh->Vertices.Data()) };
  std::shared_ptr<Ibo> ibo;
  if (mesh->Indices.Size()) {
    ibo = Ibo::Create(mesh->Indices.Size(),
                      mesh->Indices.Data(),
                      *GLIndexTypeFromStride(mesh->Indices.Stride()));
  }
  return Create(make_span(mesh->Layouts), make_span(slots), ibo);
}
void
Vao::Bind()
{
  glBindVertexArray(vao_);
}
void
Vao::Unbind()
{
  glBindVertexArray(0);
}
void
Vao::Draw(uint32_t mode, uint32_t count, uint32_t offsetBytes)
{
  Bind();
  if (ibo_) {
    glDrawElements(mode,
                   count,
                   ibo_->valuetype_,
                   reinterpret_cast<void*>(static_cast<uint64_t>(offsetBytes)));
  } else {
    glDrawArrays(mode, offsetBytes, count);
  }
  Unbind();
}
void
Vao::DrawInstance(uint32_t primcount, uint32_t count, uint32_t offsetBytes)
{
  Bind();
  if (ibo_) {
    glDrawElementsInstanced(
      GL_TRIANGLES,
      count,
      ibo_->valuetype_,
      reinterpret_cast<void*>(static_cast<uint64_t>(offsetBytes)),
      primcount);
  } else {
    throw std::runtime_error("not implemented");
  }
  Unbind();
}

} // namespace
} // namespace

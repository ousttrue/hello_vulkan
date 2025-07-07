#pragma once
#include <stdint.h>

namespace grapho {
namespace gl3 {

struct Ubo
{
  uint32_t ubo_ = 0;

  static std::shared_ptr<Ubo> Create(uint32_t size, const void* data)
  {
    auto ptr = std::make_shared<Ubo>();

    glGenBuffers(1, &ptr->ubo_);
    glBindBuffer(GL_UNIFORM_BUFFER, ptr->ubo_);
    if (data) {
      glBufferData(GL_UNIFORM_BUFFER, size, data, GL_STATIC_DRAW);
    } else {
      glBufferData(GL_UNIFORM_BUFFER, size, data, GL_DYNAMIC_DRAW);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    return ptr;
  }
  template<typename T>
  static std::shared_ptr<Ubo> Create()
  {
    return Create(sizeof(T), nullptr);
  }

  void Bind() { glBindBuffer(GL_UNIFORM_BUFFER, ubo_); }
  void Unbind() { glBindBuffer(GL_UNIFORM_BUFFER, 0); }
  void Upload(uint32_t size, const void* data)
  {
    Bind();
    glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);
    Unbind();
  }
  template<typename T>
  void Upload(const T& data)
  {
    Upload(sizeof(T), &data);
  }
  void SetBindingPoint(uint32_t binding_point)
  {
    glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, ubo_);
  }
};

}
}

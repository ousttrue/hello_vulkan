#pragma once

namespace vuloxr {

namespace gl {

struct DepthTexture : vuloxr::NonCopyable {
  uint32_t id;
  DepthTexture(int width, int height) {
    glGenRenderbuffers(1, &this->id);
    bind();
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    unbind();
  }
  void bind() { glBindRenderbuffer(GL_RENDERBUFFER, this->id); }
  void unbind() { glBindRenderbuffer(GL_RENDERBUFFER, 0); }
};

struct FramebufferObject : vuloxr::NonCopyable {
  uint32_t id;
  FramebufferObject() { glGenFramebuffers(1, &this->id); }
  ~FramebufferObject() {}
  void bind() { glBindFramebuffer(GL_FRAMEBUFFER, this->id); }
  void unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
  void attach(uint32_t color_id, uint32_t depth_id) {
    bind();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           color_id, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depth_id);
    GLenum stat = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    assert(stat == GL_FRAMEBUFFER_COMPLETE);
    unbind();
  }
};

struct RenderTarget : vuloxr::NonCopyable {
  DepthTexture depth;
  FramebufferObject fbo;

  RenderTarget(uint32_t image_id, int width, int height)
      : depth(width, height) {
    fbo.attach(image_id, this->depth.id);
    vuloxr::Logger::Info("SwapchainImage FBO:%d, TEXC:%d, TEXZ:%d, WH(%d, %d)",
                         this->fbo.id, image_id, this->depth.id, width, height);
  }

  void beginFrame(int width, int height, const XrColor4f &clearColor) {
    this->fbo.bind();
    glViewport(0, 0, width, height);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  void endFrame() { this->fbo.unbind(); }
};

struct Vbo {
  uint32_t id;
  uint32_t drawCount = 0;
  Vbo() { glGenBuffers(1, &this->id); }
  ~Vbo() {}

  void bind() { glBindBuffer(GL_ARRAY_BUFFER, this->id); }
  void unbind() { glBindBuffer(GL_ARRAY_BUFFER, 0); }
  void assign(const void *p, uint32_t size, uint32_t drawCount) {
    bind();
    glBufferData(GL_ARRAY_BUFFER, size, p, GL_STATIC_DRAW);
    unbind();
    this->drawCount = drawCount;
  }
  template <typename T> void assign(std::span<const T> data) {
    assign(data.data(), sizeof(T) * data.size());
  }
};

struct Vao {
  struct AttributeLayout {
    int32_t attribute;
    uint32_t vbo;
    uint32_t components;
    uint32_t offset;
  };
  uint32_t id;
  Vao() { glGenVertexArrays(1, &this->id); }
  ~Vao() {}
  void bind() { glBindVertexArray(this->id); }
  void unbind() { glBindVertexArray(0); }
  void assign(uint32_t stride, std::span<const AttributeLayout> layouts) {
    bind();
    for (auto &layout : layouts) {
      glEnableVertexAttribArray(layout.attribute);
      glBindBuffer(GL_ARRAY_BUFFER, layout.vbo);
      glVertexAttribPointer(layout.attribute, layout.components, GL_FLOAT,
                            GL_FALSE, stride,
                            (const void *)(int64_t)layout.offset);
    }
    unbind();
  }
};

} // namespace gl

} // namespace vuloxr

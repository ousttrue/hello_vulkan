#include "ViewRenderer.h"
#include "util_shader.h"

#include <vuloxr/gl.h>

static const shader_obj_t &getShader(const char *vs = nullptr,
                                     const char *fs = nullptr) {
  static bool s_initialized = false;
  static shader_obj_t s_sobj;
  if (!s_initialized) {
    generate_shader(&s_sobj, vs, fs);
    s_initialized = true;
    vuloxr::Logger::Info("generate_shader");
  }
  return s_sobj;
}

struct Impl {
  uint32_t swapchainWidth;
  uint32_t swapchainHeight;
  std::vector<std::shared_ptr<vuloxr::gl::RenderTarget>> backbuffers;

  vuloxr::gl::Vbo vbo;
  vuloxr::gl::Vao vao;
  struct RenderTarget {};

  Impl(const Graphics *g) {}

  ~Impl() {}

  void initSwapchain(int width, int height,
                     std::span<const SwapchainImageType> images) {
    this->swapchainWidth = width;
    this->swapchainHeight = height;

    for (auto &image : images) {
      this->backbuffers.push_back(std::make_shared<vuloxr::gl::RenderTarget>(
          image.image, width, height));
    }
  }

  void initScene(const char *vs, const char *fs,
                 std::span<const VertexAttributeLayout> layouts,
                 const void *vertices, uint32_t vertexCount) {
    auto sobj = &getShader(vs, fs);

    uint32_t stride = 0;
    std::vector<vuloxr::gl::Vao::AttributeLayout> attributes;
    for (int i = 0; i < layouts.size(); ++i) {
      auto &layout = layouts[i];
      stride += sizeof(float) * layout.components;

      attributes.push_back({
          .attribute = i == 0 ? sobj->loc_vtx : sobj->loc_clr,
          .vbo = this->vbo.id,
          .components = layout.components,
          .offset = static_cast<uint32_t>(layout.offset),
      });
    }
    this->vbo.assign(vertices, stride * vertexCount, vertexCount);
    this->vao.assign(stride, attributes);
  }

  void render(uint32_t index, const XrColor4f &clearColor) {
    auto sobj = &getShader();
    auto &backbuffer = this->backbuffers[index];
    backbuffer->beginFrame(this->swapchainWidth, this->swapchainHeight,
                           clearColor);

    glUseProgram(sobj->program);

    this->vao.bind();

    glDrawArrays(GL_TRIANGLE_STRIP, 0, this->vbo.drawCount);

    vao.unbind();
    backbuffer->endFrame();
  }
};

ViewRenderer::ViewRenderer(const Graphics *_graphics)
    : _impl(new Impl(_graphics)) {}

ViewRenderer::~ViewRenderer() { delete this->_impl; }

void ViewRenderer::initSwapchain(int width, int height,
                                 std::span<const SwapchainImageType> images) {
  this->_impl->initSwapchain(width, height, images);
}

void ViewRenderer::initScene(const char *vs, const char *fs,
                             std::span<const VertexAttributeLayout> layouts,
                             const void *vertices, uint32_t vertexCount) {
  this->_impl->initScene(vs, fs, layouts, vertices, vertexCount);
}

void ViewRenderer::render(uint32_t index, const XrColor4f &clearColor) {
  this->_impl->render(index, clearColor);
}

#include "../xr_main_loop.h"

#include <vuloxr/gl.h>

struct ShaderProgram {};

struct Impl {
  uint32_t swapchainWidth;
  uint32_t swapchainHeight;
  std::vector<std::shared_ptr<vuloxr::gl::RenderTarget>> backbuffers;

  vuloxr::gl::ShaderProgram shader;
  vuloxr::gl::Vbo vbo;
  vuloxr::gl::Ibo ibo;
  vuloxr::gl::Vao vao;
  std::shared_ptr<vuloxr::gl::Ubo> ubo;
  struct RenderTarget {};

  Impl(const Graphics *g) {}

  ~Impl() {}

  void initScene(const char *vs, const char *fs,
                 std::span<const VertexAttributeLayout> layouts,
                 const InputData &vertices, const InputData &indices) {
    const char *vsSrc[]{vs};
    const char *fsSrc[]{fs};
    assert(this->shader.compile(vsSrc, fsSrc));
    this->vbo.assign(vertices.data, vertices.byteSize(), vertices.drawCount);
    this->ibo.assign(indices.data, indices.byteSize(), indices.drawCount);

    std::vector<vuloxr::gl::Vao::AttributeLayout> attributes;
    for (int i = 0; i < layouts.size(); ++i) {
      auto &layout = layouts[i];
      attributes.push_back({
          .attribute = i,
          .vbo = this->vbo.id,
          .components = layout.components,
          .offset = static_cast<uint32_t>(layout.offset),
      });
    }
    this->vao.assign(vertices.stride, attributes, this->ibo.id);

    this->ubo = vuloxr::gl::Ubo::Create(4 * 16, nullptr);
  }

  void initSwapchain(int width, int height,
                     std::span<const SwapchainImageType> images) {
    this->swapchainWidth = width;
    this->swapchainHeight = height;

    for (auto &image : images) {
      this->backbuffers.push_back(std::make_shared<vuloxr::gl::RenderTarget>(
          image.image, width, height));
    }
  }

  void render(uint32_t index, const XrColor4f &clearColor,
              std::span<const DirectX::XMFLOAT4X4> matrices) {
    auto &backbuffer = this->backbuffers[index];
    backbuffer->beginFrame(this->swapchainWidth, this->swapchainHeight,
                           clearColor);

    this->shader.bind();
    {
      this->vao.bind();

      this->ubo->Bind();
      this->ubo->SetBindingPoint(0);

      for (auto matrix : matrices) {
        this->ubo->Upload(matrix);

        this->ibo.draw();
      }

      vao.unbind();
    }
    this->shader.unbind();

    backbuffer->endFrame();
  }
};

ViewRenderer::ViewRenderer(const Graphics *g,
                           const std::shared_ptr<GraphicsSwapchain> &swapchain)
    : _impl(new Impl(g)) {}

ViewRenderer::~ViewRenderer() { delete this->_impl; }

void ViewRenderer::initScene(const char *vs, const char *fs,
                             std::span<const VertexAttributeLayout> layouts,
                             const InputData &vertices,
                             const InputData &indices) {
  this->_impl->initScene(vs, fs, layouts, vertices, indices);
}
void ViewRenderer::initSwapchain(int width, int height,
                                 std::span<const SwapchainImageType> images) {
  this->_impl->initSwapchain(width, height, images);
}
void ViewRenderer::render(uint32_t index, const XrColor4f &clearColor,
                          std::span<const DirectX::XMFLOAT4X4> matrices) {
  this->_impl->render(index, clearColor, matrices);
}

#pragma once
#include "../xr_main_loop.h"

struct VertexAttributeLayout {
  uint32_t components;
  size_t offset;
};

struct ViewRenderer {
  struct Impl *_impl;
  ViewRenderer(const Graphics *_graphics,
               const std::shared_ptr<GraphicsSwapchain> &swapchain);
  ~ViewRenderer();
  void initScene(const char *vs, const char *fs,
                 std::span<const VertexAttributeLayout> layouts,
                 const void *vertices, uint32_t vertexCount);
  void initSwapchain(int width, int height,
                     std::span<const SwapchainImageType> images);
  void render(uint32_t index, const XrColor4f &clearColor);
};

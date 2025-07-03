#pragma once
#include <stdint.h>
#include <unordered_map>

class ViewRenderer {
  struct render_target {
    uint32_t depth_texture;
    uint32_t fbo;
  };

  std::unordered_map<uint32_t, render_target> m_renderTargetMap;

public:
  ViewRenderer();
  ~ViewRenderer();
  void render(uint32_t swapchainImage, uint32_t w, uint32_t h);
};

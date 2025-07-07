#pragma once
#include "Bvh.h"
#include <DirectXMath.h>
#include <chrono>
#include <cuber/mesh.h>
#include <functional>
#include <span>
#include <string_view>

using RenderTime = std::chrono::duration<float, std::ratio<1, 1>>;

class BvhPanel
{
  struct BvhPanelImpl* m_impl = nullptr;

public:
  BvhPanel();
  ~BvhPanel();
  void SetBvh(const std::shared_ptr<Bvh>& bvh);
  void UpdateGui();
  std::span<const cuber::Instance> GetCubes();
  void GetCubes(std::vector<cuber::Instance> &cubes);
};

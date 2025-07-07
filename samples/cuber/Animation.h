#pragma once
#include "Bvh.h"
#include <DirectXMath.h>
#include <chrono>
#include <functional>
#include <span>
#include <string_view>

namespace asio {
class io_context;
} // namespace asio

class Animation {
  struct AnimationImpl *impl_ = nullptr;

public:
  using OnFrameFunc = std::function<void(const BvhFrame &frame)>;

  Animation(asio::io_context &io);
  ~Animation();
  void SetBvh(const std::shared_ptr<Bvh> &bvh);
  void OnFrame(const OnFrameFunc &onFrame);
  void Stop();
};

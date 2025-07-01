#pragma once
#include <functional>
#include <optional>

namespace vuloxr {
namespace gui {

struct MouseState {
  float x;
  float y;
  bool leftDown = false;
  bool middleDown = false;
  bool rightDown = false;
  float wheel = 0;
};

struct WindowState {
  int width;
  int height;
  MouseState mouse;
};

using WindowLoopOnce = std::function<std::optional<WindowState>()>;

} // namespace gui
} // namespace vuloxr

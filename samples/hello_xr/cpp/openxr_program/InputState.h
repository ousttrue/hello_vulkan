#pragma once
#include <array>
#include <openxr/openxr.h>

enum Side {
  LEFT = 0,
  RIGHT = 1,
  COUNT = 2,
};

struct InputState {
  XrActionSet actionSet{XR_NULL_HANDLE};
  XrAction grabAction{XR_NULL_HANDLE};
  XrAction poseAction{XR_NULL_HANDLE};
  XrAction vibrateAction{XR_NULL_HANDLE};
  XrAction quitAction{XR_NULL_HANDLE};
  std::array<XrPath, Side::COUNT> handSubactionPath;
  std::array<XrSpace, Side::COUNT> handSpace;
  std::array<float, Side::COUNT> handScale = {{1.0f, 1.0f}};
  std::array<XrBool32, Side::COUNT> handActive;
};

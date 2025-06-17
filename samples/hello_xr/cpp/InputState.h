#pragma once
#include <array>
#include <openxr/openxr.h>
#include <vulkan/vulkan_core.h>

enum Side {
  LEFT = 0,
  RIGHT = 1,
  COUNT = 2,
};

class InputState {
  XrActionSet actionSet{XR_NULL_HANDLE};
  XrAction grabAction{XR_NULL_HANDLE};
  XrAction poseAction{XR_NULL_HANDLE};
  XrAction vibrateAction{XR_NULL_HANDLE};
  XrAction quitAction{XR_NULL_HANDLE};
  std::array<XrPath, Side::COUNT> handSubactionPath;

public:
  struct HandState {
    XrBool32 active = false;
    XrSpace space = XR_NULL_HANDLE;
    float scale = 1.0f;
  };
  std::array<HandState, Side::COUNT> hands;
  InputState(XrInstance instance, XrSession session);
  ~InputState();
  void Log(XrSession session);
  void PollActions(XrSession session);
};

#pragma once
#include <FrameParams.h>
#include <openxr/openxr.h>

class Renderer {
  struct Impl *_impl = nullptr;

public:
  Renderer();
  ~Renderer();
  void appInit(XrInstance instance);
  void appShutdown();
  void sessionInit();
  void sessionInitHand(
      int handIndex, XrHandTrackerEXT handTracker,
      XrHandJointLocationEXT jointLocations[XR_HAND_JOINT_COUNT_EXT]);

  void sessionEnd();

  void
  updateHand(int handIndex,
             XrHandJointLocationEXT jointLocations[XR_HAND_JOINT_COUNT_EXT]);

  void render(const OVRFW::ovrApplFrameIn &in, OVRFW::ovrRendererOutput &out,
              bool handTrackedL, bool handTrackedR);
};

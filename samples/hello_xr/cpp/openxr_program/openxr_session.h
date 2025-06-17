#pragma once
#include "InputState.h"
#include "SwapchainConfiguration.h"
#include "SessionEventState.h"
#include <openxr/openxr.h>
#include <vector>
#include <vulkan/vulkan.h>

class OpenXrSession {
  const struct Options &m_options;
  XrInstance m_instance;
  XrSystemId m_systemId{XR_NULL_SYSTEM_ID};

  OpenXrSession(const Options &options, XrInstance instance,
                XrSystemId systemId, XrSession session);

public:
  SessionEventState m_state;
  XrSession m_session;
  XrSpace m_appSpace;
  InputState m_input;
  std::vector<XrView> m_views;

  static OpenXrSession create(const Options &options, XrInstance instance,
                              XrSystemId systemId, VkInstance vkInstance,
                              VkPhysicalDevice vkPhysicalDevice,
                              uint32_t vkQueueFamilyIndex, VkDevice vkDevice);
  ~OpenXrSession();
  OpenXrSession(const OpenXrSession &) = delete;
  OpenXrSession &operator=(const OpenXrSession &) = delete;

  SwapchainConfiguration GetSwapchainConfiguration();

  bool LocateView(XrSpace appSpace, XrTime predictedDisplayTime,
                  XrViewConfigurationType viewConfigType,
                  uint32_t *viewCountOutput);

  // Create and submit a frame.
  XrFrameState BeginFrame();
  void EndFrame(XrTime predictedDisplayTime,
                const std::vector<XrCompositionLayerBaseHeader *> &layers);
};

#pragma once
#include "InputState.h"
#include <map>
#include <memory>
#include <openxr/openxr.h>
#include <vector>

class ProjectionLayer {
  int64_t m_colorSwapchainFormat{-1};

  std::vector<XrViewConfigurationView> m_configViews;
  std::vector<XrView> m_views;

  struct Swapchain {
    XrSwapchain handle;
    XrExtent2Di extent;
  };
  std::vector<Swapchain> m_swapchains;
  std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader *>>
      m_swapchainImages;

  XrCompositionLayerProjection m_layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  std::vector<XrCompositionLayerProjectionView> m_projectionLayerViews;

  ProjectionLayer() = default;

public:
  ~ProjectionLayer();

  static std::shared_ptr<ProjectionLayer>
  Create(XrInstance instance, XrSystemId systemId, XrSession session,
         XrViewConfigurationType viewConfigurationType,
         const std::shared_ptr<struct VulkanGraphicsPlugin> &vulkan);

  XrCompositionLayerProjection *
  RenderLayer(XrSession session, XrTime predictedDisplayTime, XrSpace appSpace,
              XrViewConfigurationType viewConfigType,
              const std::vector<XrSpace> &visualizedSpaces,
              const InputState &input,
              const std::shared_ptr<struct VulkanGraphicsPlugin> &vulkan,
              XrEnvironmentBlendMode environmentBlendMode);
};

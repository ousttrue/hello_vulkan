#pragma once
#include "FloatTypes.h"
#include "InputState.h"
#include <list>
#include <map>
#include <memory>
#include <openxr/openxr.h>
#include <vector>
#include <vulkan/vulkan.h>

class ProjectionLayer {
  int64_t m_colorSwapchainFormat{-1};

  std::vector<XrViewConfigurationView> m_configViews;
  std::vector<XrView> m_views;

  std::list<std::shared_ptr<class SwapchainImageContext>>
      m_swapchainImageContexts;
  std::map<const XrSwapchainImageBaseHeader *,
           std::shared_ptr<SwapchainImageContext>>
      m_swapchainImageContextMap;

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
         const std::shared_ptr<class VulkanGraphicsPlugin> &vulkan);

  // Allocate space for the swapchain image structures. These are different for
  // each graphics API. The returned pointers are valid for the lifetime of the
  // graphics plugin.
  std::vector<XrSwapchainImageBaseHeader *> AllocateSwapchainImageStructs(
      uint32_t capacity, VkExtent2D size, VkFormat format,
      VkSampleCountFlagBits sampleCount,
      const std::shared_ptr<class VulkanGraphicsPlugin> &vulkan);

  XrCompositionLayerProjection *RenderLayer(
      XrSession session, XrTime predictedDisplayTime, XrSpace appSpace,
      XrViewConfigurationType viewConfigType,
      const std::vector<XrSpace> &visualizedSpaces, const InputState &input,
      const std::shared_ptr<class VulkanGraphicsPlugin> &vulkan,
      const Vec4 clearColor, XrEnvironmentBlendMode environmentBlendMode);
};

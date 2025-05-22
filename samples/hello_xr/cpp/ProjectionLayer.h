#pragma once
#include <common/xr_linear.h>
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

public:
  struct Swapchain {
    XrSwapchain handle;
    XrExtent2Di extent;
  };
  std::vector<Swapchain> m_swapchains;
  std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader *>>
      m_swapchainImages;

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

  bool LocateView(XrSession session, XrSpace appSpace,
                  XrTime predictedDisplayTime,
                  XrViewConfigurationType viewConfigType,
                  uint32_t *viewCountOutput);

  struct ViewSwapchainInfo {
    std::shared_ptr<SwapchainImageContext> Swapchain;
    uint32_t ImageIndex;
    XrCompositionLayerProjectionView CompositionLayer;

    XrMatrix4x4f calcViewProjection() const {
      // Compute the view-projection transform. Note all matrixes
      // (including OpenXR's) are column-major, right-handed.
      XrMatrix4x4f proj;
      XrMatrix4x4f_CreateProjectionFov(
          &proj, GRAPHICS_VULKAN, this->CompositionLayer.fov, 0.05f, 100.0f);
      XrMatrix4x4f toView;
      XrMatrix4x4f_CreateFromRigidTransform(&toView,
                                            &this->CompositionLayer.pose);
      XrMatrix4x4f view;
      XrMatrix4x4f_InvertRigidBody(&view, &toView);
      XrMatrix4x4f vp;
      XrMatrix4x4f_Multiply(&vp, &proj, &view);

      return vp;
    }
  };
  ViewSwapchainInfo AcquireSwapchainForView(uint32_t viewIndex);
  void EndSwapchain(XrSwapchain swapchain);
};

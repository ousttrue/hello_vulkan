#pragma once
#include "SwapchainConfiguration.h"
#include "common/xr_linear.h"
#include <list>
#include <map>
#include <memory>
#include <openxr/openxr.h>
#include <vector>
#include <vulkan/vulkan.h>

struct ViewSwapchainInfo {
  std::shared_ptr<class SwapchainImageContext> Swapchain;
  uint32_t ImageIndex;
  XrCompositionLayerProjectionView CompositionLayer;

  XrMatrix4x4f calcViewProjection() const {
    // Compute the view-projection transform. Note all matrixes
    // (including OpenXR's) are column-major, right-handed.
    XrMatrix4x4f proj;
    XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_VULKAN,
                                     this->CompositionLayer.fov, 0.05f, 100.0f);
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

class ProjectionLayer {
  struct Swapchain {
    XrSwapchain handle;
    XrExtent2Di extent;
  };
  std::vector<Swapchain> m_swapchains;
  std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader *>>
      m_swapchainImages;

  std::vector<XrView> m_views;
  std::list<std::shared_ptr<class SwapchainImageContext>>
      m_swapchainImageContexts;
  std::map<const XrSwapchainImageBaseHeader *,
           std::shared_ptr<SwapchainImageContext>>
      m_swapchainImageContextMap;

  ProjectionLayer();

public:
  ~ProjectionLayer();
  static std::shared_ptr<ProjectionLayer>
  Create(XrSession session,
         const std::shared_ptr<class VulkanGraphicsPlugin> &vulkan,
         const SwapchainConfiguration &config, int64_t colorSwapchainFormat);

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

  ViewSwapchainInfo AcquireSwapchainForView(uint32_t viewIndex);
  void EndSwapchain(XrSwapchain swapchain);
};

class LayerComposition {
  std::vector<XrCompositionLayerBaseHeader *> layers;
  XrCompositionLayerProjection layer;
  std::vector<XrCompositionLayerProjectionView> projectionLayerViews;

public:
  LayerComposition(XrEnvironmentBlendMode blendMode, XrSpace appSpace) {
    this->layer = {
        .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
        .layerFlags = static_cast<XrCompositionLayerFlags>(
            blendMode == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND
                ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT |
                      XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT
                : 0),
        .space = appSpace,
        .viewCount = 0,
        .views = nullptr,
    };
  }

  // left / right
  void pushView(const XrCompositionLayerProjectionView &view) {
    this->projectionLayerViews.push_back(view);
  }

  const std::vector<XrCompositionLayerBaseHeader *> &commitLayers() {
    this->layers.clear();
    if (projectionLayerViews.size()) {
      this->layer.viewCount =
          static_cast<uint32_t>(projectionLayerViews.size());
      this->layer.views = projectionLayerViews.data();
      this->layers.push_back(
          reinterpret_cast<XrCompositionLayerBaseHeader *>(&layer));
    }
    return this->layers;
  }
};

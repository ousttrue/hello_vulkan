#pragma once
#include <vulkan/vulkan.h>
#ifdef XR_USE_PLATFORM_WIN32
#include <Unknwn.h>
#endif
#ifdef XR_USE_PLATFORM_ANDROID
#include <android_native_app_glue.h>
#endif
#include <openxr/openxr_platform.h>

#include "common/xr_linear.h"
#include <memory>
#include <openxr/openxr.h>
#include <vector>
#include <vulkan/vulkan.h>

struct ViewSwapchainInfo {
  VkImage Image;
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

class Swapchain {

public:
  XrSwapchainCreateInfo m_swapchainCreateInfo;
  XrSwapchain m_swapchain;
  std::vector<XrSwapchainImageVulkan2KHR> m_swapchainImages;

public:
  ~Swapchain();
  static std::shared_ptr<Swapchain> Create(XrSession session, uint32_t i,
                                           const XrViewConfigurationView &vp,
                                           int64_t format);

  ViewSwapchainInfo AcquireSwapchainForView(const XrView &view);
  void EndSwapchain();
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

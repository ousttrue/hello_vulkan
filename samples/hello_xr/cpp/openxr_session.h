#pragma once
#include "InputState.h"
#include <common/xr_linear.h>
#include <list>
#include <map>
#include <memory>
#include <openxr/openxr.h>
#include <vector>
#include <vulkan/vulkan.h>

class VulkanGraphicsPlugin;

class OpenXrSession {
  const struct Options &m_options;
  XrInstance m_instance;
  XrSystemId m_systemId{XR_NULL_SYSTEM_ID};

public:
  XrSession m_session;
  XrSpace m_appSpace;
  std::vector<XrSpace> m_visualizedSpaces;
  InputState m_input;

private:
  // Application's current lifecycle state according to the runtime
  XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};
  bool m_sessionRunning{false};

  XrEventDataBuffer m_eventDataBuffer;

  int64_t m_colorSwapchainFormat{-1};
  std::vector<XrViewConfigurationView> m_configViews;
  std::vector<XrView> m_views;

  std::list<std::shared_ptr<class SwapchainImageContext>>
      m_swapchainImageContexts;
  std::map<const XrSwapchainImageBaseHeader *,
           std::shared_ptr<SwapchainImageContext>>
      m_swapchainImageContextMap;

public:
  OpenXrSession(const Options &options, XrInstance instance,
                XrSystemId systemId, XrSession session, XrSpace appSpace)
      : m_options(options), m_instance(instance), m_systemId(systemId),
        m_session(session), m_appSpace(appSpace) {}
  ~OpenXrSession();

  // Manage session lifecycle to track if RenderFrame should be called.
  bool IsSessionRunning() const { return m_sessionRunning; }

  // Manage session state to track if input should be processed.
  bool IsSessionFocused() const {
    return m_sessionState == XR_SESSION_STATE_FOCUSED;
  }

  // Create a Swapchain which requires coordinating with the graphics plugin to
  // select the format, getting the system graphics properties, getting the view
  // configuration and grabbing the resulting swapchain images.
  void CreateSwapchains(const std::shared_ptr<VulkanGraphicsPlugin> &vulkan);

  // Process any events in the event queue.
  void PollEvents(bool *exitRenderLoop, bool *requestRestart);

  // Sample input actions and generate haptic feedback.
  void PollActions();

  bool LocateView(XrSession session, XrSpace appSpace,
                  XrTime predictedDisplayTime,
                  XrViewConfigurationType viewConfigType,
                  uint32_t *viewCountOutput);

  // Create and submit a frame.
  XrFrameState BeginFrame();
  void EndFrame(XrTime predictedDisplayTime,
                const std::vector<XrCompositionLayerBaseHeader *> &layers);

public:
  struct Swapchain {
    XrSwapchain handle;
    XrExtent2Di extent;
  };
  std::vector<Swapchain> m_swapchains;
  std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader *>>
      m_swapchainImages;

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

  void InitializeActions();
  void CreateVisualizedSpaces();

private:
  // Allocate space for the swapchain image structures. These are different for
  // each graphics API. The returned pointers are valid for the lifetime of the
  // graphics plugin.
  std::vector<XrSwapchainImageBaseHeader *> AllocateSwapchainImageStructs(
      uint32_t capacity, VkExtent2D size, VkFormat format,
      VkSampleCountFlagBits sampleCount,
      const std::shared_ptr<class VulkanGraphicsPlugin> &vulkan);

  // Return event if one is available, otherwise return null.
  const XrEventDataBaseHeader *TryReadNextEvent();

  void HandleSessionStateChangedEvent(
      const XrEventDataSessionStateChanged &stateChangedEvent,
      bool *exitRenderLoop, bool *requestRestart);
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

  const std::vector<XrCompositionLayerBaseHeader *> &commitLayer() {
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

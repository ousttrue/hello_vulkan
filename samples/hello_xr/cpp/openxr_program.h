// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "InputState.h"
#include <common/xr_linear.h>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

class OpenXrProgram {
  const struct Options &m_options;
  XrInstance m_instance;
  XrSystemId m_systemId{XR_NULL_SYSTEM_ID};
  const std::set<XrEnvironmentBlendMode> m_acceptableBlendModes;

public:
  XrSession m_session{XR_NULL_HANDLE};
  XrSpace m_appSpace{XR_NULL_HANDLE};

  std::vector<XrSpace> m_visualizedSpaces;

  // Application's current lifecycle state according to the runtime
  XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};
  bool m_sessionRunning{false};

  XrEventDataBuffer m_eventDataBuffer;
  InputState m_input;

  OpenXrProgram(const Options &options, XrInstance instance,
                XrSystemId systemId);

public:
  ~OpenXrProgram();

  // Create an Instance and other basic instance-level initialization.
  static std::shared_ptr<OpenXrProgram>
  Create(const Options &options,
         const std::vector<std::string> &platformExtensions, void *next);

  // Initialize the graphics device for the selected system.
  std::shared_ptr<class VulkanGraphicsPlugin>
  InitializeDevice(const std::vector<const char *> &layers,
                   const std::vector<const char *> &instanceExtensions,
                   const std::vector<const char *> &deviceExtensions);

  // Create a Session and other basic session-level initialization.
  void InitializeSession(const std::shared_ptr<VulkanGraphicsPlugin> &vulkan);

  // Create a Swapchain which requires coordinating with the graphics plugin to
  // select the format, getting the system graphics properties, getting the view
  // configuration and grabbing the resulting swapchain images.
  void CreateSwapchains(const std::shared_ptr<VulkanGraphicsPlugin> &vulkan);

  // Process any events in the event queue.
  void PollEvents(bool *exitRenderLoop, bool *requestRestart);

  // Manage session lifecycle to track if RenderFrame should be called.
  bool IsSessionRunning() const { return m_sessionRunning; }

  // Manage session state to track if input should be processed.
  bool IsSessionFocused() const {
    return m_sessionState == XR_SESSION_STATE_FOCUSED;
  }

  // Sample input actions and generate haptic feedback.
  void PollActions();

  // Create and submit a frame.
  XrFrameState BeginFrame();
  void EndFrame(XrTime predictedDisplayTime,
                const std::vector<XrCompositionLayerBaseHeader *> &layers);

  // Get preferred blend mode based on the view configuration specified in the
  // Options
  XrEnvironmentBlendMode GetPreferredBlendMode() const;

private:
  void InitializeActions();

  void CreateVisualizedSpaces();

  // Return event if one is available, otherwise return null.
  const XrEventDataBaseHeader *TryReadNextEvent();

  void HandleSessionStateChangedEvent(
      const XrEventDataSessionStateChanged &stateChangedEvent,
      bool *exitRenderLoop, bool *requestRestart);

  void LogActionSourceName(XrAction action,
                           const std::string &actionName) const;

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

  // static std::shared_ptr<ProjectionLayer>
  // Create(XrInstance instance, XrSystemId systemId, XrSession session,
  //        XrViewConfigurationType viewConfigurationType,
  //        const std::shared_ptr<class VulkanGraphicsPlugin> &vulkan);

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

private:
  // Allocate space for the swapchain image structures. These are different for
  // each graphics API. The returned pointers are valid for the lifetime of the
  // graphics plugin.
  std::vector<XrSwapchainImageBaseHeader *> AllocateSwapchainImageStructs(
      uint32_t capacity, VkExtent2D size, VkFormat format,
      VkSampleCountFlagBits sampleCount,
      const std::shared_ptr<class VulkanGraphicsPlugin> &vulkan);
};

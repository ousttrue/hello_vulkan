// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "platformplugin.h"
#include <array>
#include <map>
#include <set>

class OpenXrProgram {
  void LogInstanceInfo();
  void CreateInstanceInternal();
  void LogViewConfigurations();
  void LogEnvironmentBlendMode(XrViewConfigurationType type);
  void LogReferenceSpaces();

  enum Side {
    LEFT = 0,
    RIGHT = 1,
    COUNT = 2,
  };

  struct InputState {
    XrActionSet actionSet{XR_NULL_HANDLE};
    XrAction grabAction{XR_NULL_HANDLE};
    XrAction poseAction{XR_NULL_HANDLE};
    XrAction vibrateAction{XR_NULL_HANDLE};
    XrAction quitAction{XR_NULL_HANDLE};
    std::array<XrPath, Side::COUNT> handSubactionPath;
    std::array<XrSpace, Side::COUNT> handSpace;
    std::array<float, Side::COUNT> handScale = {{1.0f, 1.0f}};
    std::array<XrBool32, Side::COUNT> handActive;
  };

  const struct Options &m_options;
  std::shared_ptr<IPlatformPlugin> m_platformPlugin;
  std::shared_ptr<struct VulkanGraphicsPlugin> m_graphicsPlugin;
  XrInstance m_instance{XR_NULL_HANDLE};
  XrSession m_session{XR_NULL_HANDLE};
  XrSpace m_appSpace{XR_NULL_HANDLE};
  XrSystemId m_systemId{XR_NULL_SYSTEM_ID};

  std::vector<XrViewConfigurationView> m_configViews;

  struct Swapchain {
    XrSwapchain handle;
    int32_t width;
    int32_t height;
  };
  std::vector<Swapchain> m_swapchains;
  std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader *>>
      m_swapchainImages;
  std::vector<XrView> m_views;
  int64_t m_colorSwapchainFormat{-1};

  std::vector<XrSpace> m_visualizedSpaces;

  // Application's current lifecycle state according to the runtime
  XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};
  bool m_sessionRunning{false};

  XrEventDataBuffer m_eventDataBuffer;
  InputState m_input;

  const std::set<XrEnvironmentBlendMode> m_acceptableBlendModes;

public:
  OpenXrProgram(const Options &options,
                const std::shared_ptr<IPlatformPlugin> &platformPlugin,
                const std::shared_ptr<VulkanGraphicsPlugin> &graphicsPlugin);

  ~OpenXrProgram();

  // Create an Instance and other basic instance-level initialization.
  void CreateInstance();

  // Select a System for the view configuration specified in the Options
  void InitializeSystem();

  // Initialize the graphics device for the selected system.
  void InitializeDevice();

  // Create a Session and other basic session-level initialization.
  void InitializeSession();

  // Create a Swapchain which requires coordinating with the graphics plugin to
  // select the format, getting the system graphics properties, getting the view
  // configuration and grabbing the resulting swapchain images.
  void CreateSwapchains();

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
  void RenderFrame();

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

  bool RenderLayer(
      XrTime predictedDisplayTime,
      std::vector<XrCompositionLayerProjectionView> &projectionLayerViews,
      XrCompositionLayerProjection &layer);
};

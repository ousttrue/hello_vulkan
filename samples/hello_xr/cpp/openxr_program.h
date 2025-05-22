// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "FloatTypes.h"
#include "InputState.h"
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

class OpenXrProgram {
  const struct Options &m_options;

  XrInstance m_instance;
  XrSession m_session{XR_NULL_HANDLE};
  XrSpace m_appSpace{XR_NULL_HANDLE};
  XrSystemId m_systemId{XR_NULL_SYSTEM_ID};

  std::shared_ptr<class ProjectionLayer> m_projectionLayer;

  std::vector<XrSpace> m_visualizedSpaces;

  // Application's current lifecycle state according to the runtime
  XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};
  bool m_sessionRunning{false};

  XrEventDataBuffer m_eventDataBuffer;
  InputState m_input;

  const std::set<XrEnvironmentBlendMode> m_acceptableBlendModes;

  OpenXrProgram(const Options &options, XrInstance instance);

public:
  ~OpenXrProgram();

  // Create an Instance and other basic instance-level initialization.
  static std::shared_ptr<OpenXrProgram>
  Create(const Options &options,
                 const std::vector<std::string> &platformExtensions,
                 void *next);

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
  void RenderFrame(const std::shared_ptr<VulkanGraphicsPlugin> &vulkan,
                   const Vec4 &clearColor);

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
};

#pragma once
#include "InputState.h"
#include "SwapchainConfiguration.h"
#include <openxr/openxr.h>
#include <vector>
#include <vulkan/vulkan.h>

class OpenXrSession {
  const struct Options &m_options;
  XrInstance m_instance;
  XrSystemId m_systemId{XR_NULL_SYSTEM_ID};
  // Application's current lifecycle state according to the runtime
  XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};
  bool m_sessionRunning{false};
  XrEventDataBuffer m_eventDataBuffer;

public:
  XrSession m_session;
  XrSpace m_appSpace;
  std::vector<XrSpace> m_visualizedSpaces;
  InputState m_input;
  std::vector<XrView> m_views;

  OpenXrSession(const Options &options, XrInstance instance,
                XrSystemId systemId, XrSession session, XrSpace appSpace);
  ~OpenXrSession();

  // Manage session lifecycle to track if RenderFrame should be called.
  bool IsSessionRunning() const { return m_sessionRunning; }

  // Manage session state to track if input should be processed.
  bool IsSessionFocused() const {
    return m_sessionState == XR_SESSION_STATE_FOCUSED;
  }

  SwapchainConfiguration GetSwapchainConfiguration();

  // Process any events in the event queue.
  void PollEvents(bool *exitRenderLoop, bool *requestRestart);

  // Sample input actions and generate haptic feedback.
  void PollActions();

  bool LocateView(XrSpace appSpace, XrTime predictedDisplayTime,
                  XrViewConfigurationType viewConfigType,
                  uint32_t *viewCountOutput);

  // Create and submit a frame.
  XrFrameState BeginFrame();
  void EndFrame(XrTime predictedDisplayTime,
                const std::vector<XrCompositionLayerBaseHeader *> &layers);

  void InitializeActions();
  void CreateVisualizedSpaces();

private:
  // Return event if one is available, otherwise return null.
  const XrEventDataBaseHeader *TryReadNextEvent();

  void HandleSessionStateChangedEvent(
      const XrEventDataSessionStateChanged &stateChangedEvent,
      bool *exitRenderLoop, bool *requestRestart);
};

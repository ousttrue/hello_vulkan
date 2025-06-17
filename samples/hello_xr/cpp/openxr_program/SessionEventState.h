#pragma once
#include <openxr/openxr.h>

class SessionEventState {
  XrInstance m_instance;
  XrSession m_session;
  XrViewConfigurationType m_viewConfigurationType;

  // Application's current lifecycle state according to the runtime
  XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};
  bool m_sessionRunning{false};
  XrEventDataBuffer m_eventDataBuffer;

public:
  SessionEventState(XrInstance instance, XrSession session,
                    XrViewConfigurationType viewConfigurationType)
      : m_instance(instance), m_session(session),
        m_viewConfigurationType(viewConfigurationType) {}

  // Manage session lifecycle to track if RenderFrame should be called.
  bool IsSessionRunning() const { return m_sessionRunning; }

  // Manage session state to track if input should be processed.
  bool IsSessionFocused() const {
    return m_sessionState == XR_SESSION_STATE_FOCUSED;
  }

  // Process any events in the event queue.
  struct PollResult {
    bool exitRenderLoop;
    bool requestRestart;
  };
  PollResult PollEvents();

private:
  // Return event if one is available, otherwise return null.
  const XrEventDataBaseHeader *TryReadNextEvent();

  void HandleSessionStateChangedEvent(
      const XrEventDataSessionStateChanged &stateChangedEvent,
      bool *exitRenderLoop, bool *requestRestart);
};

#include "SessionEventState.h"
#include "vko/vko.h"
#include <xro/xro.h>

SessionEventState::PollResult SessionEventState::PollEvents() {
  PollResult res{
      .exitRenderLoop = false,
      .requestRestart = false,
  };

  // Process all pending messages.
  while (const XrEventDataBaseHeader *event = TryReadNextEvent()) {
    switch (event->type) {
    case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
      const auto &instanceLossPending =
          *reinterpret_cast<const XrEventDataInstanceLossPending *>(event);
      xro::Logger::Error("XrEventDataInstanceLossPending by %lld",
                         instanceLossPending.lossTime);
      return {true, true};
    }

    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
      auto sessionStateChangedEvent =
          *reinterpret_cast<const XrEventDataSessionStateChanged *>(event);
      HandleSessionStateChangedEvent(sessionStateChangedEvent,
                                     &res.exitRenderLoop, &res.requestRestart);
      break;
    }

    case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
      // m_input.Log(m_session);
      break;

    // case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
    default: {
      xro::Logger::Info("Ignoring event type %d", event->type);
      break;
    }
    }
  }

  return res;
}

// Return event if one is available, otherwise return null.
const XrEventDataBaseHeader *SessionEventState::TryReadNextEvent() {
  // It is sufficient to clear the just the XrEventDataBuffer header to
  // XR_TYPE_EVENT_DATA_BUFFER
  XrEventDataBaseHeader *baseHeader =
      reinterpret_cast<XrEventDataBaseHeader *>(&m_eventDataBuffer);
  *baseHeader = {XR_TYPE_EVENT_DATA_BUFFER};
  const XrResult xr = xrPollEvent(m_instance, &m_eventDataBuffer);
  if (xr == XR_SUCCESS) {
    if (baseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
      const XrEventDataEventsLost *const eventsLost =
          reinterpret_cast<const XrEventDataEventsLost *>(baseHeader);
      xro::Logger::Error("%d events lost", eventsLost->lostEventCount);
    }

    return baseHeader;
  }
  if (xr == XR_EVENT_UNAVAILABLE) {
    return nullptr;
  }
  XRO_THROW(xr, "xrPollEvent");
}

void SessionEventState::HandleSessionStateChangedEvent(
    const XrEventDataSessionStateChanged &stateChangedEvent,
    bool *exitRenderLoop, bool *requestRestart) {
  const XrSessionState oldState = m_sessionState;
  m_sessionState = stateChangedEvent.state;

  // xro::Logger::Info("XrEventDataSessionStateChanged: state %s->%s
  // session=%lld "
  //                   "time=%lld",
  //                   to_string(oldState), to_string(m_sessionState),
  //                   stateChangedEvent.session, stateChangedEvent.time);

  if ((stateChangedEvent.session != XR_NULL_HANDLE) &&
      (stateChangedEvent.session != m_session)) {
    xro::Logger::Error("XrEventDataSessionStateChanged for unknown session");
    return;
  }

  switch (m_sessionState) {
  case XR_SESSION_STATE_READY: {
    VKO_ASSERT(m_session != XR_NULL_HANDLE);
    XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
    sessionBeginInfo.primaryViewConfigurationType = m_viewConfigurationType;
    XRO_CHECK(xrBeginSession(m_session, &sessionBeginInfo));
    m_sessionRunning = true;
    break;
  }
  case XR_SESSION_STATE_STOPPING: {
    VKO_ASSERT(m_session != XR_NULL_HANDLE);
    m_sessionRunning = false;
    XRO_CHECK(xrEndSession(m_session))
    break;
  }
  case XR_SESSION_STATE_EXITING: {
    *exitRenderLoop = true;
    // Do not attempt to restart because user closed this session.
    *requestRestart = false;
    break;
  }
  case XR_SESSION_STATE_LOSS_PENDING: {
    *exitRenderLoop = true;
    // Poll for a new instance.
    *requestRestart = true;
    break;
  }
  default:
    break;
  }
}

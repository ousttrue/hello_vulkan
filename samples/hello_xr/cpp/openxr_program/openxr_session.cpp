#include <vulkan/vulkan.h>
#ifdef XR_USE_PLATFORM_ANDROID
#include <android_native_app_glue.h>
#endif
#ifdef XR_USE_PLATFORM_WIN32
#include <Unknwn.h>
#endif
#include <openxr/openxr_platform.h>

#include "GetXrReferenceSpaceCreateInfo.h"
#include "openxr_session.h"
#include "options.h"
#include "xr_check.h"
#include <algorithm>
#include <common/fmt.h>
#include <common/logger.h>

static void LogReferenceSpaces(XrSession m_session) {

  CHECK(m_session != XR_NULL_HANDLE);

  uint32_t spaceCount;
  CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, 0, &spaceCount, nullptr));
  std::vector<XrReferenceSpaceType> spaces(spaceCount);
  CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, spaceCount, &spaceCount,
                                         spaces.data()));

  Log::Write(Log::Level::Info,
             Fmt("Available reference spaces: %d", spaceCount));
  for (XrReferenceSpaceType space : spaces) {
    Log::Write(Log::Level::Verbose, Fmt("  Name: %s", to_string(space)));
  }
}

OpenXrSession::OpenXrSession(const Options &options, XrInstance instance,
                             XrSystemId systemId, XrSession session)
    : m_options(options), m_instance(instance), m_systemId(systemId),
      m_session(session) {
  {
    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo =
        GetXrReferenceSpaceCreateInfo(options.AppSpace);
    CHECK_XRCMD(xrCreateReferenceSpace(
        this->m_session, &referenceSpaceCreateInfo, &this->m_appSpace));
  }

  // Note: No other view configurations exist at the time this code was
  // written. If this condition is not met, the project will need to be
  // audited to see how support should be added.
  CHECK_MSG(m_options.Parsed.ViewConfigType ==
                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            "Unsupported view configuration type");

  this->m_input.InitializeActions(m_instance, m_session);
}

OpenXrSession OpenXrSession::create(const Options &options, XrInstance instance,
                                    XrSystemId systemId, VkInstance vkInstance,
                                    VkPhysicalDevice vkPhysicalDevice,
                                    uint32_t vkQueueFamilyIndex,
                                    VkDevice vkDevice) {
  XrSession session;
  {
    Log::Write(Log::Level::Verbose, Fmt("Creating session..."));

    XrGraphicsBindingVulkan2KHR graphicsBinding{
        .type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR,
        .next = nullptr,
        .instance = vkInstance,
        .physicalDevice = vkPhysicalDevice,
        .device = vkDevice,
        .queueFamilyIndex = vkQueueFamilyIndex,
        .queueIndex = 0,
    };
    XrSessionCreateInfo createInfo{
        .type = XR_TYPE_SESSION_CREATE_INFO,
        .next = &graphicsBinding,
        .systemId = systemId,
    };
    CHECK_XRCMD(xrCreateSession(instance, &createInfo, &session));
  }

  LogReferenceSpaces(session);

  return OpenXrSession(options, instance, systemId, session);
}

OpenXrSession::~OpenXrSession() {
  if (m_appSpace != XR_NULL_HANDLE) {
    xrDestroySpace(m_appSpace);
  }

  if (m_session != XR_NULL_HANDLE) {
    xrDestroySession(m_session);
  }
}

static int64_t SelectColorSwapchainFormat(const std::vector<int64_t> &formats) {
  // List of supported color swapchain formats.
  constexpr int64_t SupportedColorSwapchainFormats[] = {
      VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB,
      VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};

  auto swapchainFormatIt =
      std::find_first_of(formats.begin(), formats.end(),
                         std::begin(SupportedColorSwapchainFormats),
                         std::end(SupportedColorSwapchainFormats));
  if (swapchainFormatIt == formats.end()) {
    throw std::runtime_error(
        "No runtime swapchain format supported for color swapchain");
  }

  // Print swapchain formats and the selected one.
  {
    std::string swapchainFormatsString;
    for (int64_t format : formats) {
      const bool selected = format == *swapchainFormatIt;
      swapchainFormatsString += " ";
      if (selected) {
        swapchainFormatsString += "[";
      }
      swapchainFormatsString += std::to_string(format);
      if (selected) {
        swapchainFormatsString += "]";
      }
    }
    Log::Write(Log::Level::Verbose,
               Fmt("Swapchain Formats: %s", swapchainFormatsString.c_str()));
  }
  return *swapchainFormatIt;
}

SwapchainConfiguration OpenXrSession::GetSwapchainConfiguration() {
  SwapchainConfiguration config;

  // Query and cache view configuration views.
  uint32_t viewCount;
  CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId,
                                                m_options.Parsed.ViewConfigType,
                                                0, &viewCount, nullptr));
  config.Views.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  CHECK_XRCMD(xrEnumerateViewConfigurationViews(
      m_instance, m_systemId, m_options.Parsed.ViewConfigType, viewCount,
      &viewCount, config.Views.data()));

  // Select a swapchain format.
  uint32_t swapchainFormatCount;
  CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, 0, &swapchainFormatCount,
                                          nullptr));
  std::vector<int64_t> formats(swapchainFormatCount);
  // config.Formats.resize(swapchainFormatCount);
  CHECK_XRCMD(xrEnumerateSwapchainFormats(
      m_session, swapchainFormatCount, &swapchainFormatCount, formats.data()));

  config.Format = SelectColorSwapchainFormat(formats);

  m_views.resize(viewCount, {XR_TYPE_VIEW});

  return config;
}

void OpenXrSession::PollEvents(bool *exitRenderLoop, bool *requestRestart) {
  *exitRenderLoop = *requestRestart = false;

  // Process all pending messages.
  while (const XrEventDataBaseHeader *event = TryReadNextEvent()) {
    switch (event->type) {
    case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
      const auto &instanceLossPending =
          *reinterpret_cast<const XrEventDataInstanceLossPending *>(event);
      Log::Write(Log::Level::Warning,
                 Fmt("XrEventDataInstanceLossPending by %lld",
                     instanceLossPending.lossTime));
      *exitRenderLoop = true;
      *requestRestart = true;
      return;
    }
    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
      auto sessionStateChangedEvent =
          *reinterpret_cast<const XrEventDataSessionStateChanged *>(event);
      HandleSessionStateChangedEvent(sessionStateChangedEvent, exitRenderLoop,
                                     requestRestart);
      break;
    }
    case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
      m_input.Log(m_session);
      break;
    case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
    default: {
      Log::Write(Log::Level::Verbose,
                 Fmt("Ignoring event type %d", event->type));
      break;
    }
    }
  }
}

void OpenXrSession::HandleSessionStateChangedEvent(
    const XrEventDataSessionStateChanged &stateChangedEvent,
    bool *exitRenderLoop, bool *requestRestart) {
  const XrSessionState oldState = m_sessionState;
  m_sessionState = stateChangedEvent.state;

  Log::Write(Log::Level::Info,
             Fmt("XrEventDataSessionStateChanged: state %s->%s session=%lld "
                 "time=%lld",
                 to_string(oldState), to_string(m_sessionState),
                 stateChangedEvent.session, stateChangedEvent.time));

  if ((stateChangedEvent.session != XR_NULL_HANDLE) &&
      (stateChangedEvent.session != m_session)) {
    Log::Write(Log::Level::Error,
               "XrEventDataSessionStateChanged for unknown session");
    return;
  }

  switch (m_sessionState) {
  case XR_SESSION_STATE_READY: {
    CHECK(m_session != XR_NULL_HANDLE);
    XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
    sessionBeginInfo.primaryViewConfigurationType =
        m_options.Parsed.ViewConfigType;
    CHECK_XRCMD(xrBeginSession(m_session, &sessionBeginInfo));
    m_sessionRunning = true;
    break;
  }
  case XR_SESSION_STATE_STOPPING: {
    CHECK(m_session != XR_NULL_HANDLE);
    m_sessionRunning = false;
    CHECK_XRCMD(xrEndSession(m_session))
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

bool OpenXrSession::LocateView(XrSpace appSpace, XrTime predictedDisplayTime,
                               XrViewConfigurationType viewConfigType,
                               uint32_t *viewCountOutput) {
  XrViewLocateInfo viewLocateInfo{
      .type = XR_TYPE_VIEW_LOCATE_INFO,
      .viewConfigurationType = viewConfigType,
      .displayTime = predictedDisplayTime,
      .space = appSpace,
  };

  XrViewState viewState{
      .type = XR_TYPE_VIEW_STATE,
  };

  auto res = xrLocateViews(m_session, &viewLocateInfo, &viewState,
                           static_cast<uint32_t>(m_views.size()),
                           viewCountOutput, m_views.data());
  CHECK_XRRESULT(res, "xrLocateViews");
  if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
      (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
    return false; // There is no valid tracking poses for the views.
  }

  CHECK(*viewCountOutput == m_views.size());
  // CHECK(*viewCountOutput == m_configViews.size());
  // CHECK(*viewCountOutput == m_swapchains.size());

  return true;
}

XrFrameState OpenXrSession::BeginFrame() {
  CHECK(m_session != XR_NULL_HANDLE);

  XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
  XrFrameState frameState{XR_TYPE_FRAME_STATE};
  CHECK_XRCMD(xrWaitFrame(m_session, &frameWaitInfo, &frameState));

  XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
  CHECK_XRCMD(xrBeginFrame(m_session, &frameBeginInfo));

  return frameState;
}

void OpenXrSession::EndFrame(
    XrTime predictedDisplayTime,
    const std::vector<XrCompositionLayerBaseHeader *> &layers) {
  XrFrameEndInfo frameEndInfo{
      .type = XR_TYPE_FRAME_END_INFO,
      .displayTime = predictedDisplayTime,
      .environmentBlendMode = m_options.Parsed.EnvironmentBlendMode,
      .layerCount = static_cast<uint32_t>(layers.size()),
      .layers = layers.data(),
  };
  CHECK_XRCMD(xrEndFrame(m_session, &frameEndInfo));
}

// Return event if one is available, otherwise return null.
const XrEventDataBaseHeader *OpenXrSession::TryReadNextEvent() {
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
      Log::Write(Log::Level::Warning,
                 Fmt("%d events lost", eventsLost->lostEventCount));
    }

    return baseHeader;
  }
  if (xr == XR_EVENT_UNAVAILABLE) {
    return nullptr;
  }
  THROW_XR(xr, "xrPollEvent");
}

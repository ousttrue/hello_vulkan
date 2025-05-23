#include "openxr_session.h"
#include "GetXrReferenceSpaceCreateInfo.h"
#include "SwapchainImageContext.h"
#include "VulkanGraphicsPlugin.h"
#include "check.h"
#include "logger.h"
#include "options.h"
#include "to_string.h"

//
// OpenXrSession
//
OpenXrSession::~OpenXrSession() {
  for (Swapchain swapchain : m_swapchains) {
    xrDestroySwapchain(swapchain.handle);
  }

  if (m_input.actionSet != XR_NULL_HANDLE) {
    for (auto hand : {Side::LEFT, Side::RIGHT}) {
      xrDestroySpace(m_input.handSpace[hand]);
    }
    xrDestroyActionSet(m_input.actionSet);
  }

  for (XrSpace visualizedSpace : m_visualizedSpaces) {
    xrDestroySpace(visualizedSpace);
  }

  if (m_appSpace != XR_NULL_HANDLE) {
    xrDestroySpace(m_appSpace);
  }

  if (m_session != XR_NULL_HANDLE) {
    xrDestroySession(m_session);
  }
}

void OpenXrSession::CreateSwapchains(
    const std::shared_ptr<VulkanGraphicsPlugin> &vulkan) {
  CHECK(m_session != XR_NULL_HANDLE);

  // Read graphics properties for preferred swapchain length and logging.
  XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
  CHECK_XRCMD(xrGetSystemProperties(m_instance, m_systemId, &systemProperties));

  // Log system properties.
  Log::Write(Log::Level::Info,
             Fmt("System Properties: Name=%s VendorId=%d",
                 systemProperties.systemName, systemProperties.vendorId));
  Log::Write(
      Log::Level::Info,
      Fmt("System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
          systemProperties.graphicsProperties.maxSwapchainImageWidth,
          systemProperties.graphicsProperties.maxSwapchainImageHeight,
          systemProperties.graphicsProperties.maxLayerCount));
  Log::Write(
      Log::Level::Info,
      Fmt("System Tracking Properties: OrientationTracking=%s "
          "PositionTracking=%s",
          systemProperties.trackingProperties.orientationTracking == XR_TRUE
              ? "True"
              : "False",
          systemProperties.trackingProperties.positionTracking == XR_TRUE
              ? "True"
              : "False"));

  // Note: No other view configurations exist at the time this code was
  // written. If this condition is not met, the project will need to be
  // audited to see how support should be added.
  CHECK_MSG(m_options.Parsed.ViewConfigType ==
                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            "Unsupported view configuration type");

  // Query and cache view configuration views.
  uint32_t viewCount;
  CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId,
                                                m_options.Parsed.ViewConfigType,
                                                0, &viewCount, nullptr));
  m_configViews.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  CHECK_XRCMD(xrEnumerateViewConfigurationViews(
      m_instance, m_systemId, m_options.Parsed.ViewConfigType, viewCount,
      &viewCount, m_configViews.data()));

  // Create and cache view buffer for xrLocateViews later.
  m_views.resize(viewCount, {XR_TYPE_VIEW});

  // Create the swapchain and get the images.
  if (viewCount > 0) {
    // Select a swapchain format.
    uint32_t swapchainFormatCount;
    CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, 0, &swapchainFormatCount,
                                            nullptr));
    std::vector<int64_t> swapchainFormats(swapchainFormatCount);
    CHECK_XRCMD(xrEnumerateSwapchainFormats(
        m_session, (uint32_t)swapchainFormats.size(), &swapchainFormatCount,
        swapchainFormats.data()));
    CHECK(swapchainFormatCount == swapchainFormats.size());
    m_colorSwapchainFormat =
        vulkan->SelectColorSwapchainFormat(swapchainFormats);

    // Print swapchain formats and the selected one.
    {
      std::string swapchainFormatsString;
      for (int64_t format : swapchainFormats) {
        const bool selected = format == m_colorSwapchainFormat;
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

    // Create a swapchain for each view.
    for (uint32_t i = 0; i < viewCount; i++) {
      const XrViewConfigurationView &vp = m_configViews[i];
      Log::Write(Log::Level::Info,
                 Fmt("Creating swapchain for view %d with dimensions "
                     "Width=%d Height=%d SampleCount=%d",
                     i, vp.recommendedImageRectWidth,
                     vp.recommendedImageRectHeight,
                     vp.recommendedSwapchainSampleCount));

      // Create the swapchain.
      XrSwapchainCreateInfo swapchainCreateInfo{
          .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
          .createFlags = 0,
          .usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
          .format = m_colorSwapchainFormat,
          .sampleCount = VK_SAMPLE_COUNT_1_BIT,
          .width = vp.recommendedImageRectWidth,
          .height = vp.recommendedImageRectHeight,
          .faceCount = 1,
          .arraySize = 1,
          .mipCount = 1,
      };

      OpenXrSession::Swapchain swapchain{
          .extent =
              {
                  .width = static_cast<int32_t>(swapchainCreateInfo.width),
                  .height = static_cast<int32_t>(swapchainCreateInfo.height),
              },
      };
      CHECK_XRCMD(xrCreateSwapchain(m_session, &swapchainCreateInfo,
                                    &swapchain.handle));
      m_swapchains.push_back(swapchain);

      uint32_t imageCount;
      CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount,
                                             nullptr));
      // XXX This should really just return XrSwapchainImageBaseHeader*
      std::vector<XrSwapchainImageBaseHeader *> swapchainImages =
          AllocateSwapchainImageStructs(
              imageCount,
              {swapchainCreateInfo.width, swapchainCreateInfo.height},
              static_cast<VkFormat>(swapchainCreateInfo.format),
              static_cast<VkSampleCountFlagBits>(
                  swapchainCreateInfo.sampleCount),
              vulkan);
      CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, imageCount,
                                             &imageCount, swapchainImages[0]));

      m_swapchainImages.insert(
          std::make_pair(swapchain.handle, std::move(swapchainImages)));
    }
  }

  // return ptr;
}

static void LogActionSourceName(XrSession session, XrAction action,
                                const std::string &actionName) {
  XrBoundSourcesForActionEnumerateInfo getInfo = {
      XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
  getInfo.action = action;
  uint32_t pathCount = 0;
  CHECK_XRCMD(xrEnumerateBoundSourcesForAction(session, &getInfo, 0, &pathCount,
                                               nullptr));
  std::vector<XrPath> paths(pathCount);
  CHECK_XRCMD(xrEnumerateBoundSourcesForAction(
      session, &getInfo, uint32_t(paths.size()), &pathCount, paths.data()));

  std::string sourceName;
  for (uint32_t i = 0; i < pathCount; ++i) {
    constexpr XrInputSourceLocalizedNameFlags all =
        XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
        XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
        XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;

    XrInputSourceLocalizedNameGetInfo nameInfo = {
        XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
    nameInfo.sourcePath = paths[i];
    nameInfo.whichComponents = all;

    uint32_t size = 0;
    CHECK_XRCMD(
        xrGetInputSourceLocalizedName(session, &nameInfo, 0, &size, nullptr));
    if (size < 1) {
      continue;
    }
    std::vector<char> grabSource(size);
    CHECK_XRCMD(xrGetInputSourceLocalizedName(session, &nameInfo,
                                              uint32_t(grabSource.size()),
                                              &size, grabSource.data()));
    if (!sourceName.empty()) {
      sourceName += " and ";
    }
    sourceName += "'";
    sourceName += std::string(grabSource.data(), size - 1);
    sourceName += "'";
  }

  Log::Write(Log::Level::Info,
             Fmt("%s action is bound to %s", actionName.c_str(),
                 ((!sourceName.empty()) ? sourceName.c_str() : "nothing")));
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
      LogActionSourceName(m_session, m_input.grabAction, "Grab");
      LogActionSourceName(m_session, m_input.quitAction, "Quit");
      LogActionSourceName(m_session, m_input.poseAction, "Pose");
      LogActionSourceName(m_session, m_input.vibrateAction, "Vibrate");
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

void OpenXrSession::PollActions() {
  m_input.handActive = {XR_FALSE, XR_FALSE};

  // Sync actions
  const XrActiveActionSet activeActionSet{m_input.actionSet, XR_NULL_PATH};
  XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
  syncInfo.countActiveActionSets = 1;
  syncInfo.activeActionSets = &activeActionSet;
  CHECK_XRCMD(xrSyncActions(m_session, &syncInfo));

  // Get pose and grab action state and start haptic vibrate when hand is 90%
  // squeezed.
  for (auto hand : {Side::LEFT, Side::RIGHT}) {
    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = m_input.grabAction;
    getInfo.subactionPath = m_input.handSubactionPath[hand];

    XrActionStateFloat grabValue{XR_TYPE_ACTION_STATE_FLOAT};
    CHECK_XRCMD(xrGetActionStateFloat(m_session, &getInfo, &grabValue));
    if (grabValue.isActive == XR_TRUE) {
      // Scale the rendered hand by 1.0f (open) to 0.5f (fully squeezed).
      m_input.handScale[hand] = 1.0f - 0.5f * grabValue.currentState;
      if (grabValue.currentState > 0.9f) {
        XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
        vibration.amplitude = 0.5;
        vibration.duration = XR_MIN_HAPTIC_DURATION;
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

        XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
        hapticActionInfo.action = m_input.vibrateAction;
        hapticActionInfo.subactionPath = m_input.handSubactionPath[hand];
        CHECK_XRCMD(xrApplyHapticFeedback(m_session, &hapticActionInfo,
                                          (XrHapticBaseHeader *)&vibration));
      }
    }

    getInfo.action = m_input.poseAction;
    XrActionStatePose poseState{XR_TYPE_ACTION_STATE_POSE};
    CHECK_XRCMD(xrGetActionStatePose(m_session, &getInfo, &poseState));
    m_input.handActive[hand] = poseState.isActive;
  }

  // There were no subaction paths specified for the quit action, because we
  // don't care which hand did it.
  XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr,
                               m_input.quitAction, XR_NULL_PATH};
  XrActionStateBoolean quitValue{XR_TYPE_ACTION_STATE_BOOLEAN};
  CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &quitValue));
  if ((quitValue.isActive == XR_TRUE) &&
      (quitValue.changedSinceLastSync == XR_TRUE) &&
      (quitValue.currentState == XR_TRUE)) {
    CHECK_XRCMD(xrRequestExitSession(m_session));
  }
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

std::vector<XrSwapchainImageBaseHeader *>
OpenXrSession::AllocateSwapchainImageStructs(
    uint32_t capacity, VkExtent2D size, VkFormat format,
    VkSampleCountFlagBits sampleCount,
    const std::shared_ptr<class VulkanGraphicsPlugin> &vulkan) {
  // Allocate and initialize the buffer of image structs (must be sequential
  // in memory for xrEnumerateSwapchainImages). Return back an array of
  // pointers to each swapchain image struct so the consumer doesn't need to
  // know the type/size. Keep the buffer alive by adding it into the list of
  // buffers.
  m_swapchainImageContexts.emplace_back(SwapchainImageContext::Create(
      vulkan->m_vkDevice, vulkan->m_memAllocator, capacity, size, format,
      sampleCount, vulkan->m_shaderProgram));
  auto swapchainImageContext = m_swapchainImageContexts.back();

  // Map every swapchainImage base pointer to this context
  for (auto &base : swapchainImageContext->m_bases) {
    m_swapchainImageContextMap[base] = swapchainImageContext;
  }

  return swapchainImageContext->m_bases;
}

bool OpenXrSession::LocateView(XrSession session, XrSpace appSpace,
                               XrTime predictedDisplayTime,
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

  auto res = xrLocateViews(session, &viewLocateInfo, &viewState,
                           static_cast<uint32_t>(m_views.size()),
                           viewCountOutput, m_views.data());
  CHECK_XRRESULT(res, "xrLocateViews");
  if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
      (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
    return false; // There is no valid tracking poses for the views.
  }

  CHECK(*viewCountOutput == m_views.size());
  CHECK(*viewCountOutput == m_configViews.size());
  CHECK(*viewCountOutput == m_swapchains.size());

  return true;
}

void OpenXrSession::CreateVisualizedSpaces() {
  CHECK(m_session != XR_NULL_HANDLE);

  std::string visualizedSpaces[] = {
      "ViewFront",        "Local",      "Stage",
      "StageLeft",        "StageRight", "StageLeftRotated",
      "StageRightRotated"};

  for (const auto &visualizedSpace : visualizedSpaces) {
    auto referenceSpaceCreateInfo =
        GetXrReferenceSpaceCreateInfo(visualizedSpace);
    XrSpace space;
    XrResult res =
        xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &space);
    if (XR_SUCCEEDED(res)) {
      m_visualizedSpaces.push_back(space);
    } else {
      Log::Write(Log::Level::Warning,
                 Fmt("Failed to create reference space %s with error %d",
                     visualizedSpace.c_str(), res));
    }
  }
}

OpenXrSession::ViewSwapchainInfo
OpenXrSession::AcquireSwapchainForView(uint32_t i) {
  // Each view has a separate swapchain which is acquired, rendered to, and
  // released.
  const Swapchain viewSwapchain = m_swapchains[i];

  XrSwapchainImageAcquireInfo acquireInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
  };
  uint32_t swapchainImageIndex;
  CHECK_XRCMD(xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo,
                                      &swapchainImageIndex));

  XrSwapchainImageWaitInfo waitInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
      .timeout = XR_INFINITE_DURATION,
  };
  CHECK_XRCMD(xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo));

  const XrSwapchainImageBaseHeader *const swapchainImage =
      m_swapchainImages[viewSwapchain.handle][swapchainImageIndex];

  ViewSwapchainInfo info = {};

  info.CompositionLayer = {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
      .pose = m_views[i].pose,
      .fov = m_views[i].fov,
      .subImage = {
          .swapchain = viewSwapchain.handle,
          .imageRect = {.offset = {0, 0}, .extent = viewSwapchain.extent}}};

  info.Swapchain = m_swapchainImageContextMap[swapchainImage];
  info.ImageIndex = info.Swapchain->ImageIndex(swapchainImage);

  return info;
}

void OpenXrSession::EndSwapchain(XrSwapchain swapchain) {
  XrSwapchainImageReleaseInfo releaseInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
  };
  CHECK_XRCMD(xrReleaseSwapchainImage(swapchain, &releaseInfo));
}

void OpenXrSession::InitializeActions() {
  // Create an action set.
  {
    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy_s(actionSetInfo.actionSetName, "gameplay");
    strcpy_s(actionSetInfo.localizedActionSetName, "Gameplay");
    actionSetInfo.priority = 0;
    CHECK_XRCMD(
        xrCreateActionSet(m_instance, &actionSetInfo, &m_input.actionSet));
  }

  // Get the XrPath for the left and right hands - we will use them as
  // subaction paths.
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left",
                             &m_input.handSubactionPath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right",
                             &m_input.handSubactionPath[Side::RIGHT]));

  // Create actions.
  {
    // Create an input action for grabbing objects with the left and right
    // hands.
    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
    actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
    strcpy_s(actionInfo.actionName, "grab_object");
    strcpy_s(actionInfo.localizedActionName, "Grab Object");
    actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
    actionInfo.subactionPaths = m_input.handSubactionPath.data();
    CHECK_XRCMD(
        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.grabAction));

    // Create an input action getting the left and right hand poses.
    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
    strcpy_s(actionInfo.actionName, "hand_pose");
    strcpy_s(actionInfo.localizedActionName, "Hand Pose");
    actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
    actionInfo.subactionPaths = m_input.handSubactionPath.data();
    CHECK_XRCMD(
        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.poseAction));

    // Create output actions for vibrating the left and right controller.
    actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
    strcpy_s(actionInfo.actionName, "vibrate_hand");
    strcpy_s(actionInfo.localizedActionName, "Vibrate Hand");
    actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
    actionInfo.subactionPaths = m_input.handSubactionPath.data();
    CHECK_XRCMD(
        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.vibrateAction));

    // Create input actions for quitting the session using the left and right
    // controller. Since it doesn't matter which hand did this, we do not
    // specify subaction paths for it. We will just suggest bindings for both
    // hands, where possible.
    actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    strcpy_s(actionInfo.actionName, "quit_session");
    strcpy_s(actionInfo.localizedActionName, "Quit Session");
    actionInfo.countSubactionPaths = 0;
    actionInfo.subactionPaths = nullptr;
    CHECK_XRCMD(
        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.quitAction));
  }

  std::array<XrPath, Side::COUNT> selectPath;
  std::array<XrPath, Side::COUNT> squeezeValuePath;
  std::array<XrPath, Side::COUNT> squeezeForcePath;
  std::array<XrPath, Side::COUNT> squeezeClickPath;
  std::array<XrPath, Side::COUNT> posePath;
  std::array<XrPath, Side::COUNT> hapticPath;
  std::array<XrPath, Side::COUNT> menuClickPath;
  std::array<XrPath, Side::COUNT> bClickPath;
  std::array<XrPath, Side::COUNT> triggerValuePath;
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/select/click",
                             &selectPath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/select/click",
                             &selectPath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/value",
                             &squeezeValuePath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/value",
                             &squeezeValuePath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/force",
                             &squeezeForcePath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/force",
                             &squeezeForcePath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/click",
                             &squeezeClickPath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/click",
                             &squeezeClickPath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/grip/pose",
                             &posePath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/grip/pose",
                             &posePath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/output/haptic",
                             &hapticPath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/output/haptic",
                             &hapticPath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/menu/click",
                             &menuClickPath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/menu/click",
                             &menuClickPath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/b/click",
                             &bClickPath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/b/click",
                             &bClickPath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/trigger/value",
                             &triggerValuePath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/trigger/value",
                             &triggerValuePath[Side::RIGHT]));
  // Suggest bindings for KHR Simple.
  {
    XrPath khrSimpleInteractionProfilePath;
    CHECK_XRCMD(xrStringToPath(m_instance,
                               "/interaction_profiles/khr/simple_controller",
                               &khrSimpleInteractionProfilePath));
    std::vector<XrActionSuggestedBinding> bindings{
        {// Fall back to a click input for the grab action.
         {m_input.grabAction, selectPath[Side::LEFT]},
         {m_input.grabAction, selectPath[Side::RIGHT]},
         {m_input.poseAction, posePath[Side::LEFT]},
         {m_input.poseAction, posePath[Side::RIGHT]},
         {m_input.quitAction, menuClickPath[Side::LEFT]},
         {m_input.quitAction, menuClickPath[Side::RIGHT]},
         {m_input.vibrateAction, hapticPath[Side::LEFT]},
         {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};
    XrInteractionProfileSuggestedBinding suggestedBindings{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = khrSimpleInteractionProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    CHECK_XRCMD(
        xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
  }
  // Suggest bindings for the Oculus Touch.
  {
    XrPath oculusTouchInteractionProfilePath;
    CHECK_XRCMD(xrStringToPath(m_instance,
                               "/interaction_profiles/oculus/touch_controller",
                               &oculusTouchInteractionProfilePath));
    std::vector<XrActionSuggestedBinding> bindings{
        {{m_input.grabAction, squeezeValuePath[Side::LEFT]},
         {m_input.grabAction, squeezeValuePath[Side::RIGHT]},
         {m_input.poseAction, posePath[Side::LEFT]},
         {m_input.poseAction, posePath[Side::RIGHT]},
         {m_input.quitAction, menuClickPath[Side::LEFT]},
         {m_input.vibrateAction, hapticPath[Side::LEFT]},
         {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};
    XrInteractionProfileSuggestedBinding suggestedBindings{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = oculusTouchInteractionProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    CHECK_XRCMD(
        xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
  }
  // Suggest bindings for the Vive Controller.
  {
    XrPath viveControllerInteractionProfilePath;
    CHECK_XRCMD(xrStringToPath(m_instance,
                               "/interaction_profiles/htc/vive_controller",
                               &viveControllerInteractionProfilePath));
    std::vector<XrActionSuggestedBinding> bindings{
        {{m_input.grabAction, triggerValuePath[Side::LEFT]},
         {m_input.grabAction, triggerValuePath[Side::RIGHT]},
         {m_input.poseAction, posePath[Side::LEFT]},
         {m_input.poseAction, posePath[Side::RIGHT]},
         {m_input.quitAction, menuClickPath[Side::LEFT]},
         {m_input.quitAction, menuClickPath[Side::RIGHT]},
         {m_input.vibrateAction, hapticPath[Side::LEFT]},
         {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};
    XrInteractionProfileSuggestedBinding suggestedBindings{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = viveControllerInteractionProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    CHECK_XRCMD(
        xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
  }

  // Suggest bindings for the Valve Index Controller.
  {
    XrPath indexControllerInteractionProfilePath;
    CHECK_XRCMD(xrStringToPath(m_instance,
                               "/interaction_profiles/valve/index_controller",
                               &indexControllerInteractionProfilePath));
    std::vector<XrActionSuggestedBinding> bindings{
        {{m_input.grabAction, squeezeForcePath[Side::LEFT]},
         {m_input.grabAction, squeezeForcePath[Side::RIGHT]},
         {m_input.poseAction, posePath[Side::LEFT]},
         {m_input.poseAction, posePath[Side::RIGHT]},
         {m_input.quitAction, bClickPath[Side::LEFT]},
         {m_input.quitAction, bClickPath[Side::RIGHT]},
         {m_input.vibrateAction, hapticPath[Side::LEFT]},
         {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};
    XrInteractionProfileSuggestedBinding suggestedBindings{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile =
        indexControllerInteractionProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    CHECK_XRCMD(
        xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
  }

  // Suggest bindings for the Microsoft Mixed Reality Motion Controller.
  {
    XrPath microsoftMixedRealityInteractionProfilePath;
    CHECK_XRCMD(xrStringToPath(
        m_instance, "/interaction_profiles/microsoft/motion_controller",
        &microsoftMixedRealityInteractionProfilePath));
    std::vector<XrActionSuggestedBinding> bindings{
        {{m_input.grabAction, squeezeClickPath[Side::LEFT]},
         {m_input.grabAction, squeezeClickPath[Side::RIGHT]},
         {m_input.poseAction, posePath[Side::LEFT]},
         {m_input.poseAction, posePath[Side::RIGHT]},
         {m_input.quitAction, menuClickPath[Side::LEFT]},
         {m_input.quitAction, menuClickPath[Side::RIGHT]},
         {m_input.vibrateAction, hapticPath[Side::LEFT]},
         {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};
    XrInteractionProfileSuggestedBinding suggestedBindings{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile =
        microsoftMixedRealityInteractionProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    CHECK_XRCMD(
        xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
  }
  XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
  actionSpaceInfo.action = m_input.poseAction;
  actionSpaceInfo.poseInActionSpace.orientation.w = 1.f;
  actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::LEFT];
  CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo,
                                  &m_input.handSpace[Side::LEFT]));
  actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::RIGHT];
  CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo,
                                  &m_input.handSpace[Side::RIGHT]));

  XrSessionActionSetsAttachInfo attachInfo{
      XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attachInfo.countActionSets = 1;
  attachInfo.actionSets = &m_input.actionSet;
  CHECK_XRCMD(xrAttachSessionActionSets(m_session, &attachInfo));
}

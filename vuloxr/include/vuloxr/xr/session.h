#pragma once
#include "../xr.h"
#include <algorithm>

namespace vuloxr {

namespace xr {

struct Session : NonCopyable {
  XrSession session;
  operator XrSession() const { return this->session; }
  std::vector<VkFormat> formats;
  Session(XrInstance _instance, XrSystemId _systemId, VkInstance instance,
          VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex,
          VkDevice device) {
    // Log::Write(Log::Level::Verbose, Fmt("Creating session..."));
    XrGraphicsBindingVulkan2KHR graphicsBinding{
        .type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR,
        .next = nullptr,
        .instance = instance,
        .physicalDevice = physicalDevice,
        .device = device,
        .queueFamilyIndex = queueFamilyIndex,
        .queueIndex = 0,
    };
    XrSessionCreateInfo createInfo{
        .type = XR_TYPE_SESSION_CREATE_INFO,
        .next = &graphicsBinding,
        .systemId = _systemId,
    };
    CheckXrResult(xrCreateSession(_instance, &createInfo, &this->session));

    // Select a swapchain format.
    uint32_t swapchainFormatCount;
    CheckXrResult(xrEnumerateSwapchainFormats(this->session, 0,
                                              &swapchainFormatCount, nullptr));
    this->formats.resize(swapchainFormatCount);
    CheckXrResult(xrEnumerateSwapchainFormats(
        this->session, swapchainFormatCount, &swapchainFormatCount,
        (int64_t *)formats.data()));
  }
  ~Session() {
    Logger::Info("xro::Session::~Session ...");
    xrEndSession(this->session);
    xrDestroySession(this->session);
  }

  VkFormat selectColorSwapchainFormat() const {
    // List of supported color swapchain formats.
    constexpr VkFormat SupportedColorSwapchainFormats[] = {
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
    };

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
      for (auto format : this->formats) {
        const bool selected = format == *swapchainFormatIt;
        swapchainFormatsString += " ";
        if (selected) {
          swapchainFormatsString += "[";
        }
        swapchainFormatsString += magic_enum::enum_name(format);
        if (selected) {
          swapchainFormatsString += "]";
        }
      }
      Logger::Info("Swapchain Formats: %s", swapchainFormatsString.c_str());
    }
    return *swapchainFormatIt;
  }
};

inline XrFrameState beginFrame(XrSession session) {
  XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
  XrFrameState frameState{XR_TYPE_FRAME_STATE};
  CheckXrResult(xrWaitFrame(session, &frameWaitInfo, &frameState));
  XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
  CheckXrResult(xrBeginFrame(session, &frameBeginInfo));
  return frameState;
}

inline void
endFrame(XrSession session, XrTime predictedDisplayTime,
         XrEnvironmentBlendMode blendMode,
         const std::vector<XrCompositionLayerBaseHeader *> &layers) {
  XrFrameEndInfo frameEndInfo{
      .type = XR_TYPE_FRAME_END_INFO,
      .displayTime = predictedDisplayTime,
      .environmentBlendMode = blendMode,
      .layerCount = static_cast<uint32_t>(layers.size()),
      .layers = layers.data(),
  };
  CheckXrResult(xrEndFrame(session, &frameEndInfo));
}

struct Stereoscope {
  std::vector<XrViewConfigurationView> viewConfigurations;
  std::vector<XrView> views;

  Stereoscope(XrInstance instance, XrSystemId systemId,
              XrViewConfigurationType viewConfigurationType) {
    // Query and cache view configuration views.
    uint32_t viewCount;
    CheckXrResult(xrEnumerateViewConfigurationViews(
        instance, systemId, viewConfigurationType, 0, &viewCount, nullptr));
    this->viewConfigurations.resize(viewCount,
                                    {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    this->views.resize(viewCount, {XR_TYPE_VIEW});
    CheckXrResult(xrEnumerateViewConfigurationViews(
        instance, systemId, viewConfigurationType, viewCount, &viewCount,
        viewConfigurations.data()));
  }

  bool Locate(XrSession session, XrSpace appSpace, XrTime predictedDisplayTime,
              XrViewConfigurationType viewConfigType) {
    XrViewLocateInfo viewLocateInfo{
        .type = XR_TYPE_VIEW_LOCATE_INFO,
        .viewConfigurationType = viewConfigType,
        .displayTime = predictedDisplayTime,
        .space = appSpace,
    };

    XrViewState viewState{
        .type = XR_TYPE_VIEW_STATE,
    };

    uint32_t viewCountOutput;
    auto res = xrLocateViews(session, &viewLocateInfo, &viewState,
                             static_cast<uint32_t>(this->views.size()),
                             &viewCountOutput, this->views.data());
    // CHECK_XRRESULT(res, "xrLocateViews");
    if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
        (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
      return false; // There is no valid tracking poses for the views.
    }

    // CHECK(*viewCountOutput == this->views.size());
    // CHECK(*viewCountOutput == m_configViews.size());
    // CHECK(*viewCountOutput == m_swapchains.size());

    return true;
  }
};

inline std::tuple<XrResult, XrSpaceLocation>
locate(XrSpace world, XrTime predictedDisplayTime, XrSpace target) {
  XrSpaceLocation spaceLocation{
      .type = XR_TYPE_SPACE_LOCATION,
  };
  auto res = xrLocateSpace(target, world, predictedDisplayTime, &spaceLocation);
  return {res, spaceLocation};
}

inline bool locationIsValid(XrResult res, const XrSpaceLocation &location) {
  if (!XR_UNQUALIFIED_SUCCESS(res)) {
    return false;
  }
  if ((location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) == 0) {
    return false;
  }
  if ((location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) == 0) {
    return false;
  }
  return true;
}

class SessionState {
  XrInstance m_instance;
  XrSession m_session;
  XrViewConfigurationType m_viewConfigurationType;

  // Application's current lifecycle state according to the runtime
  XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;
  XrEventDataBuffer m_eventDataBuffer;

public:
  bool m_sessionRunning = false;
  SessionState(XrInstance instance, XrSession session,
               XrViewConfigurationType viewConfigurationType)
      : m_instance(instance), m_session(session),
        m_viewConfigurationType(viewConfigurationType) {}

  // Process any events in the event queue.
  struct PollResult {
    bool exitRenderLoop;
    bool requestRestart;
    bool isSessionRunning;
  };
  PollResult PollEvents() {
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
        Logger::Error("XrEventDataInstanceLossPending by %lld",
                      instanceLossPending.lossTime);
        return {true, true, m_sessionRunning};
      }

      case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
        auto sessionStateChangedEvent =
            *reinterpret_cast<const XrEventDataSessionStateChanged *>(event);
        HandleSessionStateChangedEvent(
            sessionStateChangedEvent, &res.exitRenderLoop, &res.requestRestart);
        break;
      }

      case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
        // m_input.Log(m_session);
        break;

      // case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
      default: {
        Logger::Info("Ignoring event type %d", event->type);
        break;
      }
      }
    }

    res.isSessionRunning = m_sessionRunning;
    return res;
  }

private:
  // Return event if one is available, otherwise return null.
  const XrEventDataBaseHeader *TryReadNextEvent() {
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
        Logger::Error("%d events lost", eventsLost->lostEventCount);
      }

      return baseHeader;
    }
    if (xr == XR_EVENT_UNAVAILABLE) {
      return nullptr;
    }
    ThrowXrResult(xr, "xrPollEvent");
  }
  void HandleSessionStateChangedEvent(
      const XrEventDataSessionStateChanged &stateChangedEvent,
      bool *exitRenderLoop, bool *requestRestart) {
    const XrSessionState oldState = m_sessionState;
    m_sessionState = stateChangedEvent.state;

    Logger::Info("XrEventDataSessionStateChanged: state %s->%s session = % lld "
                 "time=%lld",
                 magic_enum::enum_name(oldState).data(),
                 magic_enum::enum_name(m_sessionState).data(),
                 stateChangedEvent.session, stateChangedEvent.time);

    if ((stateChangedEvent.session != XR_NULL_HANDLE) &&
        (stateChangedEvent.session != m_session)) {
      Logger::Error("XrEventDataSessionStateChanged for unknown session");
      return;
    }

    switch (m_sessionState) {
    case XR_SESSION_STATE_READY: {
      assert(m_session != XR_NULL_HANDLE);
      XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
      sessionBeginInfo.primaryViewConfigurationType = m_viewConfigurationType;
      CheckXrResult(xrBeginSession(m_session, &sessionBeginInfo));
      m_sessionRunning = true;
      break;
    }
    case XR_SESSION_STATE_STOPPING: {
      assert(m_session != XR_NULL_HANDLE);
      m_sessionRunning = false;
      CheckXrResult(xrEndSession(m_session));
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
};

enum Side {
  LEFT = 0,
  RIGHT = 1,
  COUNT = 2,
};

struct HandState {
  XrBool32 active = false;
  XrSpace space = XR_NULL_HANDLE;
  float scale = 1.0f;
};

struct InputState {
  XrSession session;
  XrActionSet actionSet{XR_NULL_HANDLE};
  XrAction grabAction{XR_NULL_HANDLE};
  XrAction poseAction{XR_NULL_HANDLE};
  XrAction vibrateAction{XR_NULL_HANDLE};
  XrAction quitAction{XR_NULL_HANDLE};
  std::array<XrPath, Side::COUNT> handSubactionPath;
  std::array<HandState, Side::COUNT> hands;

  static void str_cpy(char *dst, const char *src) {
    memcpy(dst, src, strlen(src + 1));
  }

  InputState(XrInstance instance, XrSession _session) : session(_session) {
    // Create an action set.
    {
      XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
      str_cpy(actionSetInfo.actionSetName, "gameplay");
      str_cpy(actionSetInfo.localizedActionSetName, "Gameplay");
      actionSetInfo.priority = 0;
      CheckXrResult(
          xrCreateActionSet(instance, &actionSetInfo, &this->actionSet));
    }

    // Get the XrPath for the left and right hands - we will use them as
    // subaction paths.
    CheckXrResult(xrStringToPath(instance, "/user/hand/left",
                                 &this->handSubactionPath[Side::LEFT]));
    CheckXrResult(xrStringToPath(instance, "/user/hand/right",
                                 &this->handSubactionPath[Side::RIGHT]));

    // Create actions.
    {
      // Create an input action for grabbing objects with the left and right
      // hands.
      XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
      actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
      str_cpy(actionInfo.actionName, "grab_object");
      str_cpy(actionInfo.localizedActionName, "Grab Object");
      actionInfo.countSubactionPaths = uint32_t(this->handSubactionPath.size());
      actionInfo.subactionPaths = this->handSubactionPath.data();
      CheckXrResult(
          xrCreateAction(this->actionSet, &actionInfo, &this->grabAction));

      // Create an input action getting the left and right hand poses.
      actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
      str_cpy(actionInfo.actionName, "hand_pose");
      str_cpy(actionInfo.localizedActionName, "Hand Pose");
      actionInfo.countSubactionPaths = uint32_t(this->handSubactionPath.size());
      actionInfo.subactionPaths = this->handSubactionPath.data();
      CheckXrResult(
          xrCreateAction(this->actionSet, &actionInfo, &this->poseAction));

      // Create output actions for vibrating the left and right controller.
      actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
      str_cpy(actionInfo.actionName, "vibrate_hand");
      str_cpy(actionInfo.localizedActionName, "Vibrate Hand");
      actionInfo.countSubactionPaths = uint32_t(this->handSubactionPath.size());
      actionInfo.subactionPaths = this->handSubactionPath.data();
      CheckXrResult(
          xrCreateAction(this->actionSet, &actionInfo, &this->vibrateAction));

      // Create input actions for quitting the session using the left and right
      // controller. Since it doesn't matter which hand did this, we do not
      // specify subaction paths for it. We will just suggest bindings for both
      // hands, where possible.
      actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
      str_cpy(actionInfo.actionName, "quit_session");
      str_cpy(actionInfo.localizedActionName, "Quit Session");
      actionInfo.countSubactionPaths = 0;
      actionInfo.subactionPaths = nullptr;
      CheckXrResult(
          xrCreateAction(this->actionSet, &actionInfo, &this->quitAction));
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
    CheckXrResult(xrStringToPath(instance, "/user/hand/left/input/select/click",
                                 &selectPath[Side::LEFT]));
    CheckXrResult(xrStringToPath(instance,
                                 "/user/hand/right/input/select/click",
                                 &selectPath[Side::RIGHT]));
    CheckXrResult(xrStringToPath(instance,
                                 "/user/hand/left/input/squeeze/value",
                                 &squeezeValuePath[Side::LEFT]));
    CheckXrResult(xrStringToPath(instance,
                                 "/user/hand/right/input/squeeze/value",
                                 &squeezeValuePath[Side::RIGHT]));
    CheckXrResult(xrStringToPath(instance,
                                 "/user/hand/left/input/squeeze/force",
                                 &squeezeForcePath[Side::LEFT]));
    CheckXrResult(xrStringToPath(instance,
                                 "/user/hand/right/input/squeeze/force",
                                 &squeezeForcePath[Side::RIGHT]));
    CheckXrResult(xrStringToPath(instance,
                                 "/user/hand/left/input/squeeze/click",
                                 &squeezeClickPath[Side::LEFT]));
    CheckXrResult(xrStringToPath(instance,
                                 "/user/hand/right/input/squeeze/click",
                                 &squeezeClickPath[Side::RIGHT]));
    CheckXrResult(xrStringToPath(instance, "/user/hand/left/input/grip/pose",
                                 &posePath[Side::LEFT]));
    CheckXrResult(xrStringToPath(instance, "/user/hand/right/input/grip/pose",
                                 &posePath[Side::RIGHT]));
    CheckXrResult(xrStringToPath(instance, "/user/hand/left/output/haptic",
                                 &hapticPath[Side::LEFT]));
    CheckXrResult(xrStringToPath(instance, "/user/hand/right/output/haptic",
                                 &hapticPath[Side::RIGHT]));
    CheckXrResult(xrStringToPath(instance, "/user/hand/left/input/menu/click",
                                 &menuClickPath[Side::LEFT]));
    CheckXrResult(xrStringToPath(instance, "/user/hand/right/input/menu/click",
                                 &menuClickPath[Side::RIGHT]));
    CheckXrResult(xrStringToPath(instance, "/user/hand/left/input/b/click",
                                 &bClickPath[Side::LEFT]));
    CheckXrResult(xrStringToPath(instance, "/user/hand/right/input/b/click",
                                 &bClickPath[Side::RIGHT]));
    CheckXrResult(xrStringToPath(instance,
                                 "/user/hand/left/input/trigger/value",
                                 &triggerValuePath[Side::LEFT]));
    CheckXrResult(xrStringToPath(instance,
                                 "/user/hand/right/input/trigger/value",
                                 &triggerValuePath[Side::RIGHT]));
    // Suggest bindings for KHR Simple.
    {
      XrPath khrSimpleInteractionProfilePath;
      CheckXrResult(xrStringToPath(
          instance, "/interaction_profiles/khr/simple_controller",
          &khrSimpleInteractionProfilePath));
      std::vector<XrActionSuggestedBinding> bindings{
          {// Fall back to a click input for the grab action.
           {this->grabAction, selectPath[Side::LEFT]},
           {this->grabAction, selectPath[Side::RIGHT]},
           {this->poseAction, posePath[Side::LEFT]},
           {this->poseAction, posePath[Side::RIGHT]},
           {this->quitAction, menuClickPath[Side::LEFT]},
           {this->quitAction, menuClickPath[Side::RIGHT]},
           {this->vibrateAction, hapticPath[Side::LEFT]},
           {this->vibrateAction, hapticPath[Side::RIGHT]}}};
      XrInteractionProfileSuggestedBinding suggestedBindings{
          XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
      suggestedBindings.interactionProfile = khrSimpleInteractionProfilePath;
      suggestedBindings.suggestedBindings = bindings.data();
      suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
      CheckXrResult(
          xrSuggestInteractionProfileBindings(instance, &suggestedBindings));
    }
    // Suggest bindings for the Oculus Touch.
    {
      XrPath oculusTouchInteractionProfilePath;
      CheckXrResult(xrStringToPath(
          instance, "/interaction_profiles/oculus/touch_controller",
          &oculusTouchInteractionProfilePath));
      std::vector<XrActionSuggestedBinding> bindings{
          {{this->grabAction, squeezeValuePath[Side::LEFT]},
           {this->grabAction, squeezeValuePath[Side::RIGHT]},
           {this->poseAction, posePath[Side::LEFT]},
           {this->poseAction, posePath[Side::RIGHT]},
           {this->quitAction, menuClickPath[Side::LEFT]},
           {this->vibrateAction, hapticPath[Side::LEFT]},
           {this->vibrateAction, hapticPath[Side::RIGHT]}}};
      XrInteractionProfileSuggestedBinding suggestedBindings{
          XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
      suggestedBindings.interactionProfile = oculusTouchInteractionProfilePath;
      suggestedBindings.suggestedBindings = bindings.data();
      suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
      CheckXrResult(
          xrSuggestInteractionProfileBindings(instance, &suggestedBindings));
    }
    // Suggest bindings for the Vive Controller.
    {
      XrPath viveControllerInteractionProfilePath;
      CheckXrResult(xrStringToPath(instance,
                                   "/interaction_profiles/htc/vive_controller",
                                   &viveControllerInteractionProfilePath));
      std::vector<XrActionSuggestedBinding> bindings{
          {{this->grabAction, triggerValuePath[Side::LEFT]},
           {this->grabAction, triggerValuePath[Side::RIGHT]},
           {this->poseAction, posePath[Side::LEFT]},
           {this->poseAction, posePath[Side::RIGHT]},
           {this->quitAction, menuClickPath[Side::LEFT]},
           {this->quitAction, menuClickPath[Side::RIGHT]},
           {this->vibrateAction, hapticPath[Side::LEFT]},
           {this->vibrateAction, hapticPath[Side::RIGHT]}}};
      XrInteractionProfileSuggestedBinding suggestedBindings{
          XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
      suggestedBindings.interactionProfile =
          viveControllerInteractionProfilePath;
      suggestedBindings.suggestedBindings = bindings.data();
      suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
      CheckXrResult(
          xrSuggestInteractionProfileBindings(instance, &suggestedBindings));
    }

    // Suggest bindings for the Valve Index Controller.
    {
      XrPath indexControllerInteractionProfilePath;
      CheckXrResult(xrStringToPath(
          instance, "/interaction_profiles/valve/index_controller",
          &indexControllerInteractionProfilePath));
      std::vector<XrActionSuggestedBinding> bindings{
          {{this->grabAction, squeezeForcePath[Side::LEFT]},
           {this->grabAction, squeezeForcePath[Side::RIGHT]},
           {this->poseAction, posePath[Side::LEFT]},
           {this->poseAction, posePath[Side::RIGHT]},
           {this->quitAction, bClickPath[Side::LEFT]},
           {this->quitAction, bClickPath[Side::RIGHT]},
           {this->vibrateAction, hapticPath[Side::LEFT]},
           {this->vibrateAction, hapticPath[Side::RIGHT]}}};
      XrInteractionProfileSuggestedBinding suggestedBindings{
          XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
      suggestedBindings.interactionProfile =
          indexControllerInteractionProfilePath;
      suggestedBindings.suggestedBindings = bindings.data();
      suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
      CheckXrResult(
          xrSuggestInteractionProfileBindings(instance, &suggestedBindings));
    }

    // Suggest bindings for the Microsoft Mixed Reality Motion Controller.
    {
      XrPath microsoftMixedRealityInteractionProfilePath;
      CheckXrResult(xrStringToPath(
          instance, "/interaction_profiles/microsoft/motion_controller",
          &microsoftMixedRealityInteractionProfilePath));
      std::vector<XrActionSuggestedBinding> bindings{
          {{this->grabAction, squeezeClickPath[Side::LEFT]},
           {this->grabAction, squeezeClickPath[Side::RIGHT]},
           {this->poseAction, posePath[Side::LEFT]},
           {this->poseAction, posePath[Side::RIGHT]},
           {this->quitAction, menuClickPath[Side::LEFT]},
           {this->quitAction, menuClickPath[Side::RIGHT]},
           {this->vibrateAction, hapticPath[Side::LEFT]},
           {this->vibrateAction, hapticPath[Side::RIGHT]}}};
      XrInteractionProfileSuggestedBinding suggestedBindings{
          XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
      suggestedBindings.interactionProfile =
          microsoftMixedRealityInteractionProfilePath;
      suggestedBindings.suggestedBindings = bindings.data();
      suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
      CheckXrResult(
          xrSuggestInteractionProfileBindings(instance, &suggestedBindings));
    }
    XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    actionSpaceInfo.action = this->poseAction;
    actionSpaceInfo.poseInActionSpace.orientation.w = 1.f;
    actionSpaceInfo.subactionPath = this->handSubactionPath[Side::LEFT];
    CheckXrResult(xrCreateActionSpace(this->session, &actionSpaceInfo,
                                      &this->hands[Side::LEFT].space));
    actionSpaceInfo.subactionPath = this->handSubactionPath[Side::RIGHT];
    CheckXrResult(xrCreateActionSpace(this->session, &actionSpaceInfo,
                                      &this->hands[Side::RIGHT].space));

    XrSessionActionSetsAttachInfo attachInfo{
        XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &this->actionSet;
    CheckXrResult(xrAttachSessionActionSets(this->session, &attachInfo));
  }
  ~InputState() {
    if (this->actionSet != XR_NULL_HANDLE) {
      for (auto hand : {Side::LEFT, Side::RIGHT}) {
        xrDestroySpace(this->hands[hand].space);
      }
      xrDestroyActionSet(this->actionSet);
    }
  }

  void Log() {
    LogActionSourceName(this->session, this->grabAction, "Grab");
    LogActionSourceName(this->session, this->quitAction, "Quit");
    LogActionSourceName(this->session, this->poseAction, "Pose");
    LogActionSourceName(this->session, this->vibrateAction, "Vibrate");
  }
  static void LogActionSourceName(XrSession session, XrAction action,
                                  const std::string &actionName) {
    XrBoundSourcesForActionEnumerateInfo getInfo = {
        XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
    getInfo.action = action;
    uint32_t pathCount = 0;
    CheckXrResult(xrEnumerateBoundSourcesForAction(session, &getInfo, 0,
                                                   &pathCount, nullptr));
    std::vector<XrPath> paths(pathCount);
    CheckXrResult(xrEnumerateBoundSourcesForAction(
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
      CheckXrResult(
          xrGetInputSourceLocalizedName(session, &nameInfo, 0, &size, nullptr));
      if (size < 1) {
        continue;
      }
      std::vector<char> grabSource(size);
      CheckXrResult(xrGetInputSourceLocalizedName(session, &nameInfo,
                                                  uint32_t(grabSource.size()),
                                                  &size, grabSource.data()));
      if (!sourceName.empty()) {
        sourceName += " and ";
      }
      sourceName += "'";
      sourceName += std::string(grabSource.data(), size - 1);
      sourceName += "'";
    }

    Logger::Info("%s action is bound to %s", actionName.c_str(),
                 ((!sourceName.empty()) ? sourceName.c_str() : "nothing"));
  }

  void PollActions() {
    this->hands[Side::LEFT].active = XR_FALSE;
    this->hands[Side::RIGHT].active = XR_FALSE;

    // Sync actions
    const XrActiveActionSet activeActionSet{this->actionSet, XR_NULL_PATH};
    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;
    CheckXrResult(xrSyncActions(this->session, &syncInfo));

    // Get pose and grab action state and start haptic vibrate when hand is 90%
    // squeezed.
    for (auto hand : {Side::LEFT, Side::RIGHT}) {
      XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
      getInfo.action = this->grabAction;
      getInfo.subactionPath = this->handSubactionPath[hand];

      XrActionStateFloat grabValue{XR_TYPE_ACTION_STATE_FLOAT};
      CheckXrResult(xrGetActionStateFloat(this->session, &getInfo, &grabValue));
      if (grabValue.isActive == XR_TRUE) {
        // Scale the rendered hand by 1.0f (open) to 0.5f (fully squeezed).
        this->hands[hand].scale = 1.0f - 0.5f * grabValue.currentState;
        if (grabValue.currentState > 0.9f) {
          XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
          vibration.amplitude = 0.5;
          vibration.duration = XR_MIN_HAPTIC_DURATION;
          vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

          XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
          hapticActionInfo.action = this->vibrateAction;
          hapticActionInfo.subactionPath = this->handSubactionPath[hand];
          CheckXrResult(
              xrApplyHapticFeedback(this->session, &hapticActionInfo,
                                    (XrHapticBaseHeader *)&vibration));
        }
      }

      getInfo.action = this->poseAction;
      XrActionStatePose poseState{XR_TYPE_ACTION_STATE_POSE};
      CheckXrResult(xrGetActionStatePose(this->session, &getInfo, &poseState));
      this->hands[hand].active = poseState.isActive;
    }

    // There were no subaction paths specified for the quit action, because we
    // don't care which hand did it.
    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr,
                                 this->quitAction, XR_NULL_PATH};
    XrActionStateBoolean quitValue{XR_TYPE_ACTION_STATE_BOOLEAN};
    CheckXrResult(xrGetActionStateBoolean(this->session, &getInfo, &quitValue));
    if ((quitValue.isActive == XR_TRUE) &&
        (quitValue.changedSinceLastSync == XR_TRUE) &&
        (quitValue.currentState == XR_TRUE)) {
      CheckXrResult(xrRequestExitSession(this->session));
    }
  }
};

struct Swapchain {
  std::vector<XrSwapchainImageVulkan2KHR> swapchainImages;
  XrSwapchainCreateInfo swapchainCreateInfo;
  XrSwapchain swapchain;

  Swapchain(XrSession session, uint32_t i, const XrViewConfigurationView &vp,
            int64_t format) {

    Logger::Info("Creating swapchain for view %d with dimensions "
                 "Width=%d Height=%d SampleCount=%d",
                 i, vp.recommendedImageRectWidth, vp.recommendedImageRectHeight,
                 vp.recommendedSwapchainSampleCount);

    // Create the swapchain.
    this->swapchainCreateInfo = {
        .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
        .createFlags = 0,
        .usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                      XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
        .format = format,
        .sampleCount = VK_SAMPLE_COUNT_1_BIT,
        .width = vp.recommendedImageRectWidth,
        .height = vp.recommendedImageRectHeight,
        .faceCount = 1,
        .arraySize = 1,
        .mipCount = 1,
    };
    CheckXrResult(xrCreateSwapchain(session, &this->swapchainCreateInfo,
                                    &this->swapchain));

    uint32_t imageCount;
    CheckXrResult(
        xrEnumerateSwapchainImages(this->swapchain, 0, &imageCount, nullptr));
    this->swapchainImages.resize(imageCount,
                                 {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});

    std::vector<XrSwapchainImageBaseHeader *> pointers;
    for (auto &image : this->swapchainImages) {
      pointers.push_back((XrSwapchainImageBaseHeader *)&image);
    }
    CheckXrResult(xrEnumerateSwapchainImages(this->swapchain, imageCount,
                                             &imageCount, pointers[0]));
  }

  ~Swapchain() { xrDestroySwapchain(this->swapchain); }

  std::tuple<VkImage, XrCompositionLayerProjectionView>
  AcquireSwapchain(const XrView &view) {
    XrSwapchainImageAcquireInfo acquireInfo{
        .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
    };
    uint32_t swapchainImageIndex;
    CheckXrResult(xrAcquireSwapchainImage(this->swapchain, &acquireInfo,
                                          &swapchainImageIndex));

    XrSwapchainImageWaitInfo waitInfo{
        .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
        .timeout = XR_INFINITE_DURATION,
    };
    CheckXrResult(xrWaitSwapchainImage(this->swapchain, &waitInfo));

    return {
        this->swapchainImages[swapchainImageIndex].image,
        {
            .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
            .pose = view.pose,
            .fov = view.fov,
            .subImage =
                {
                    .swapchain = this->swapchain,
                    .imageRect =
                        {
                            .offset = {0, 0},
                            .extent = {.width = static_cast<int32_t>(
                                           this->swapchainCreateInfo.width),
                                       .height = static_cast<int32_t>(
                                           this->swapchainCreateInfo.height)},
                        },
                },
        }};
  }

  void EndSwapchain() {
    XrSwapchainImageReleaseInfo releaseInfo{
        .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
    };
    CheckXrResult(xrReleaseSwapchainImage(this->swapchain, &releaseInfo));
  }

  VkExtent2D extent() const {
    return {this->swapchainCreateInfo.width, this->swapchainCreateInfo.height};
  }

  VkFormat format() const {
    return static_cast<VkFormat>(this->swapchainCreateInfo.format);
  }

  VkSampleCountFlagBits sampleCountFlagBits() const {
    return static_cast<VkSampleCountFlagBits>(
        this->swapchainCreateInfo.sampleCount);
  }
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

} // namespace xr
} // namespace vuloxr

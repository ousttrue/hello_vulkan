#include "InputState.h"
#include <string>
#include <xro/xro.h>

#if !defined(XR_USE_PLATFORM_WIN32)
#define strcpy_s(dest, source) strncpy((dest), (source), sizeof(dest))
#endif

InputState::InputState(XrInstance instance, XrSession session) {
  // Create an action set.
  {
    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy_s(actionSetInfo.actionSetName, "gameplay");
    strcpy_s(actionSetInfo.localizedActionSetName, "Gameplay");
    actionSetInfo.priority = 0;
    XRO_CHECK(xrCreateActionSet(instance, &actionSetInfo, &this->actionSet));
  }

  // Get the XrPath for the left and right hands - we will use them as
  // subaction paths.
  XRO_CHECK(xrStringToPath(instance, "/user/hand/left",
                           &this->handSubactionPath[Side::LEFT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/right",
                           &this->handSubactionPath[Side::RIGHT]));

  // Create actions.
  {
    // Create an input action for grabbing objects with the left and right
    // hands.
    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
    actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
    strcpy_s(actionInfo.actionName, "grab_object");
    strcpy_s(actionInfo.localizedActionName, "Grab Object");
    actionInfo.countSubactionPaths = uint32_t(this->handSubactionPath.size());
    actionInfo.subactionPaths = this->handSubactionPath.data();
    XRO_CHECK(xrCreateAction(this->actionSet, &actionInfo, &this->grabAction));

    // Create an input action getting the left and right hand poses.
    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
    strcpy_s(actionInfo.actionName, "hand_pose");
    strcpy_s(actionInfo.localizedActionName, "Hand Pose");
    actionInfo.countSubactionPaths = uint32_t(this->handSubactionPath.size());
    actionInfo.subactionPaths = this->handSubactionPath.data();
    XRO_CHECK(xrCreateAction(this->actionSet, &actionInfo, &this->poseAction));

    // Create output actions for vibrating the left and right controller.
    actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
    strcpy_s(actionInfo.actionName, "vibrate_hand");
    strcpy_s(actionInfo.localizedActionName, "Vibrate Hand");
    actionInfo.countSubactionPaths = uint32_t(this->handSubactionPath.size());
    actionInfo.subactionPaths = this->handSubactionPath.data();
    XRO_CHECK(
        xrCreateAction(this->actionSet, &actionInfo, &this->vibrateAction));

    // Create input actions for quitting the session using the left and right
    // controller. Since it doesn't matter which hand did this, we do not
    // specify subaction paths for it. We will just suggest bindings for both
    // hands, where possible.
    actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    strcpy_s(actionInfo.actionName, "quit_session");
    strcpy_s(actionInfo.localizedActionName, "Quit Session");
    actionInfo.countSubactionPaths = 0;
    actionInfo.subactionPaths = nullptr;
    XRO_CHECK(xrCreateAction(this->actionSet, &actionInfo, &this->quitAction));
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
  XRO_CHECK(xrStringToPath(instance, "/user/hand/left/input/select/click",
                           &selectPath[Side::LEFT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/right/input/select/click",
                           &selectPath[Side::RIGHT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/left/input/squeeze/value",
                           &squeezeValuePath[Side::LEFT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/right/input/squeeze/value",
                           &squeezeValuePath[Side::RIGHT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/left/input/squeeze/force",
                           &squeezeForcePath[Side::LEFT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/right/input/squeeze/force",
                           &squeezeForcePath[Side::RIGHT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/left/input/squeeze/click",
                           &squeezeClickPath[Side::LEFT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/right/input/squeeze/click",
                           &squeezeClickPath[Side::RIGHT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/left/input/grip/pose",
                           &posePath[Side::LEFT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/right/input/grip/pose",
                           &posePath[Side::RIGHT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/left/output/haptic",
                           &hapticPath[Side::LEFT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/right/output/haptic",
                           &hapticPath[Side::RIGHT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/left/input/menu/click",
                           &menuClickPath[Side::LEFT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/right/input/menu/click",
                           &menuClickPath[Side::RIGHT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/left/input/b/click",
                           &bClickPath[Side::LEFT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/right/input/b/click",
                           &bClickPath[Side::RIGHT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/left/input/trigger/value",
                           &triggerValuePath[Side::LEFT]));
  XRO_CHECK(xrStringToPath(instance, "/user/hand/right/input/trigger/value",
                           &triggerValuePath[Side::RIGHT]));
  // Suggest bindings for KHR Simple.
  {
    XrPath khrSimpleInteractionProfilePath;
    XRO_CHECK(xrStringToPath(instance,
                             "/interaction_profiles/khr/simple_controller",
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
    XRO_CHECK(
        xrSuggestInteractionProfileBindings(instance, &suggestedBindings));
  }
  // Suggest bindings for the Oculus Touch.
  {
    XrPath oculusTouchInteractionProfilePath;
    XRO_CHECK(xrStringToPath(instance,
                             "/interaction_profiles/oculus/touch_controller",
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
    XRO_CHECK(
        xrSuggestInteractionProfileBindings(instance, &suggestedBindings));
  }
  // Suggest bindings for the Vive Controller.
  {
    XrPath viveControllerInteractionProfilePath;
    XRO_CHECK(xrStringToPath(instance,
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
    suggestedBindings.interactionProfile = viveControllerInteractionProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    XRO_CHECK(
        xrSuggestInteractionProfileBindings(instance, &suggestedBindings));
  }

  // Suggest bindings for the Valve Index Controller.
  {
    XrPath indexControllerInteractionProfilePath;
    XRO_CHECK(xrStringToPath(instance,
                             "/interaction_profiles/valve/index_controller",
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
    XRO_CHECK(
        xrSuggestInteractionProfileBindings(instance, &suggestedBindings));
  }

  // Suggest bindings for the Microsoft Mixed Reality Motion Controller.
  {
    XrPath microsoftMixedRealityInteractionProfilePath;
    XRO_CHECK(xrStringToPath(
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
    XRO_CHECK(
        xrSuggestInteractionProfileBindings(instance, &suggestedBindings));
  }
  XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
  actionSpaceInfo.action = this->poseAction;
  actionSpaceInfo.poseInActionSpace.orientation.w = 1.f;
  actionSpaceInfo.subactionPath = this->handSubactionPath[Side::LEFT];
  XRO_CHECK(xrCreateActionSpace(session, &actionSpaceInfo,
                                &this->handSpace[Side::LEFT]));
  actionSpaceInfo.subactionPath = this->handSubactionPath[Side::RIGHT];
  XRO_CHECK(xrCreateActionSpace(session, &actionSpaceInfo,
                                &this->handSpace[Side::RIGHT]));

  XrSessionActionSetsAttachInfo attachInfo{
      XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attachInfo.countActionSets = 1;
  attachInfo.actionSets = &this->actionSet;
  XRO_CHECK(xrAttachSessionActionSets(session, &attachInfo));
}

InputState::~InputState() {
  if (this->actionSet != XR_NULL_HANDLE) {
    for (auto hand : {Side::LEFT, Side::RIGHT}) {
      xrDestroySpace(this->handSpace[hand]);
    }
    xrDestroyActionSet(this->actionSet);
  }
}

static void LogActionSourceName(XrSession session, XrAction action,
                                const std::string &actionName) {
  XrBoundSourcesForActionEnumerateInfo getInfo = {
      XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
  getInfo.action = action;
  uint32_t pathCount = 0;
  XRO_CHECK(xrEnumerateBoundSourcesForAction(session, &getInfo, 0, &pathCount,
                                             nullptr));
  std::vector<XrPath> paths(pathCount);
  XRO_CHECK(xrEnumerateBoundSourcesForAction(
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
    XRO_CHECK(
        xrGetInputSourceLocalizedName(session, &nameInfo, 0, &size, nullptr));
    if (size < 1) {
      continue;
    }
    std::vector<char> grabSource(size);
    XRO_CHECK(xrGetInputSourceLocalizedName(session, &nameInfo,
                                            uint32_t(grabSource.size()), &size,
                                            grabSource.data()));
    if (!sourceName.empty()) {
      sourceName += " and ";
    }
    sourceName += "'";
    sourceName += std::string(grabSource.data(), size - 1);
    sourceName += "'";
  }

  xro::Logger::Info("%s action is bound to %s", actionName.c_str(),
                    ((!sourceName.empty()) ? sourceName.c_str() : "nothing"));
}

void InputState::Log(XrSession session) {
  LogActionSourceName(session, this->grabAction, "Grab");
  LogActionSourceName(session, this->quitAction, "Quit");
  LogActionSourceName(session, this->poseAction, "Pose");
  LogActionSourceName(session, this->vibrateAction, "Vibrate");
}

void InputState::PollActions(XrSession session) {
  this->handActive = {XR_FALSE, XR_FALSE};

  // Sync actions
  const XrActiveActionSet activeActionSet{this->actionSet, XR_NULL_PATH};
  XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
  syncInfo.countActiveActionSets = 1;
  syncInfo.activeActionSets = &activeActionSet;
  XRO_CHECK(xrSyncActions(session, &syncInfo));

  // Get pose and grab action state and start haptic vibrate when hand is 90%
  // squeezed.
  for (auto hand : {Side::LEFT, Side::RIGHT}) {
    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = this->grabAction;
    getInfo.subactionPath = this->handSubactionPath[hand];

    XrActionStateFloat grabValue{XR_TYPE_ACTION_STATE_FLOAT};
    XRO_CHECK(xrGetActionStateFloat(session, &getInfo, &grabValue));
    if (grabValue.isActive == XR_TRUE) {
      // Scale the rendered hand by 1.0f (open) to 0.5f (fully squeezed).
      this->handScale[hand] = 1.0f - 0.5f * grabValue.currentState;
      if (grabValue.currentState > 0.9f) {
        XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
        vibration.amplitude = 0.5;
        vibration.duration = XR_MIN_HAPTIC_DURATION;
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

        XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
        hapticActionInfo.action = this->vibrateAction;
        hapticActionInfo.subactionPath = this->handSubactionPath[hand];
        XRO_CHECK(xrApplyHapticFeedback(session, &hapticActionInfo,
                                        (XrHapticBaseHeader *)&vibration));
      }
    }

    getInfo.action = this->poseAction;
    XrActionStatePose poseState{XR_TYPE_ACTION_STATE_POSE};
    XRO_CHECK(xrGetActionStatePose(session, &getInfo, &poseState));
    this->handActive[hand] = poseState.isActive;
  }

  // There were no subaction paths specified for the quit action, because we
  // don't care which hand did it.
  XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr,
                               this->quitAction, XR_NULL_PATH};
  XrActionStateBoolean quitValue{XR_TYPE_ACTION_STATE_BOOLEAN};
  XRO_CHECK(xrGetActionStateBoolean(session, &getInfo, &quitValue));
  if ((quitValue.isActive == XR_TRUE) &&
      (quitValue.changedSinceLastSync == XR_TRUE) &&
      (quitValue.currentState == XR_TRUE)) {
    XRO_CHECK(xrRequestExitSession(session));
  }
}

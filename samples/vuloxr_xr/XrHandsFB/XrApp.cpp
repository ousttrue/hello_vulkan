/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * Licensed under the Oculus SDK License Agreement (the "License");
 * you may not use the Oculus SDK except in compliance with the License,
 * which is provided at the time of installation or download, or which
 * otherwise accompanies this software in either electronic or hard copy form.
 *
 * You may obtain a copy of the License at
 * https://developer.oculus.com/licenses/oculussdk/
 *
 * Unless required by applicable law or agreed to in writing, the Oculus SDK
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*******************************************************************************

Filename    :   XrApp.cpp
Content     :   OpenXR application base class.
Created     :   July 2020
Authors     :   Federico Schliemann
Language    :   c++

*******************************************************************************/

#include "XrApp.h"
#include "xr_linear.h"
#include <Windows.h>
#include <assert.h>
#include <inttypes.h>
#include "OXR.h"

#if defined(ANDROID)
#include <android/native_window_jni.h>
#include <android/window.h>
#include <openxr/openxr.h>
#include <pthread.h>
#include <sys/prctl.h> // for prctl( PR_SET_NAME )
#include <unistd.h>
#elif defined(WIN32)
// Favor the high performance NVIDIA or AMD GPUs
extern "C" {
// http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
// https://gpuopen.com/learn/amdpowerxpressrequesthighperformance/
__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;
}
#endif //  defined(WIN32) defined(ANDROID)

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {
//
// #if defined(ANDROID)
// /**
//  * Process the next main command.
//  */
// static void app_handle_cmd(struct android_app* app, int32_t cmd) {
//     XrApp* appState = (XrApp*)app->userData;
//     appState->HandleAndroidCmd(app, cmd);
// }
//
// void XrApp::HandleAndroidCmd(struct android_app* app, int32_t cmd) {
//     switch (cmd) {
//         // There is no APP_CMD_CREATE. The ANativeActivity creates the
//         // application thread from onCreate(). The application thread
//         // then calls android_main().
//         case APP_CMD_START: {
//             ALOGV("onStart()");
//             ALOGV("    APP_CMD_START");
//             break;
//         }
//         case APP_CMD_RESUME: {
//             ALOGV("onResume()");
//             ALOGV("    APP_CMD_RESUME");
//             Resumed = true;
//             break;
//         }
//         case APP_CMD_PAUSE: {
//             ALOGV("onPause()");
//             ALOGV("    APP_CMD_PAUSE");
//             Resumed = false;
//             break;
//         }
//         case APP_CMD_STOP: {
//             ALOGV("onStop()");
//             ALOGV("    APP_CMD_STOP");
//             break;
//         }
//         case APP_CMD_DESTROY: {
//             ALOGV("onDestroy()");
//             ALOGV("    APP_CMD_DESTROY");
//             break;
//         }
//         case APP_CMD_INIT_WINDOW: {
//             ALOGV("surfaceCreated()");
//             ALOGV("    APP_CMD_INIT_WINDOW");
//             break;
//         }
//         case APP_CMD_TERM_WINDOW: {
//             ALOGV("surfaceDestroyed()");
//             ALOGV("    APP_CMD_TERM_WINDOW");
//             break;
//         }
//     }
// }
// #endif // defined(ANDROID)

void XrApp::HandleSessionStateChanges(XrSessionState state) {
  if (state == XR_SESSION_STATE_READY) {
#if defined(ANDROID)
    assert(Resumed);
#endif // defined(ANDROID)
    assert(SessionActive == false);

    XrSessionBeginInfo sessionBeginInfo = {XR_TYPE_SESSION_BEGIN_INFO};
    sessionBeginInfo.primaryViewConfigurationType =
        ViewportConfig.viewConfigurationType;

    XrResult result;
    OXR(result = xrBeginSession(Session, &sessionBeginInfo));
    SessionActive = (result == XR_SUCCESS);

    // Set session state once we have entered VR mode and have a valid session
    // object.
    if (SessionActive) {
#if defined(ANDROID)
      XrPerfSettingsLevelEXT cpuPerfLevel =
          XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
      switch (CpuLevel) {
      case 0:
        cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT;
        break;
      case 1:
        cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_LOW_EXT;
        break;
      case 2:
        cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
        break;
      case 3:
        cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;
        break;
      default:
        ALOGE("Invalid CPU level %d", CpuLevel);
        break;
      }

      XrPerfSettingsLevelEXT gpuPerfLevel =
          XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
      switch (GpuLevel) {
      case 0:
        gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT;
        break;
      case 1:
        gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_LOW_EXT;
        break;
      case 2:
        gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
        break;
      case 3:
        gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;
        break;
      default:
        ALOGE("Invalid GPU level %d", GpuLevel);
        break;
      }

      PFN_xrPerfSettingsSetPerformanceLevelEXT
          pfnPerfSettingsSetPerformanceLevelEXT = NULL;
      OXR(xrGetInstanceProcAddr(
          Instance, "xrPerfSettingsSetPerformanceLevelEXT",
          (PFN_xrVoidFunction *)(&pfnPerfSettingsSetPerformanceLevelEXT)));

      OXR(pfnPerfSettingsSetPerformanceLevelEXT(
          Session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, cpuPerfLevel));
      OXR(pfnPerfSettingsSetPerformanceLevelEXT(
          Session, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, gpuPerfLevel));

      PFN_xrSetAndroidApplicationThreadKHR pfnSetAndroidApplicationThreadKHR =
          NULL;
      OXR(xrGetInstanceProcAddr(
          Instance, "xrSetAndroidApplicationThreadKHR",
          (PFN_xrVoidFunction *)(&pfnSetAndroidApplicationThreadKHR)));

      OXR(pfnSetAndroidApplicationThreadKHR(
          Session, XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR, MainThreadTid));
      OXR(pfnSetAndroidApplicationThreadKHR(
          Session, XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR, RenderThreadTid));
#endif // defined(ANDROID)
    }
  } else if (state == XR_SESSION_STATE_STOPPING) {
#if defined(ANDROID)
    assert(Resumed == false);
#endif // defined(ANDROID)
    assert(SessionActive);

    OXR(xrEndSession(Session));
    SessionActive = false;
  }
}

XrResult XrApp::PollXrEvent(XrEventDataBuffer *eventDataBuffer) {
  return xrPollEvent(Instance, eventDataBuffer);
}

void XrApp::HandleXrEvents() {
  XrEventDataBuffer eventDataBuffer = {};

  // Poll for events
  for (;;) {
    XrEventDataBaseHeader *baseEventHeader =
        (XrEventDataBaseHeader *)(&eventDataBuffer);
    baseEventHeader->type = XR_TYPE_EVENT_DATA_BUFFER;
    baseEventHeader->next = NULL;
    XrResult r;
    OXR(r = PollXrEvent(&eventDataBuffer));
    if (r != XR_SUCCESS) {
      break;
    }

    AppHandleEvent(baseEventHeader);

    switch (baseEventHeader->type) {
    case XR_TYPE_EVENT_DATA_EVENTS_LOST:
      ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_EVENTS_LOST event");
      break;
    case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
      ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING "
            "event");
      break;
    case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
      ALOGV("xrPollEvent: received "
            "XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED event");
      break;
    case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: {
      const XrEventDataPerfSettingsEXT *perf_settings_event =
          (XrEventDataPerfSettingsEXT *)(baseEventHeader);
      ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT event "
            ": type % d subdomain % d : level % d->level % d ",
            perf_settings_event->type, perf_settings_event->subDomain,
            perf_settings_event->fromLevel, perf_settings_event->toLevel);
    } break;
    case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
      ALOGV("xrPollEvent: received "
            "XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING event");
      break;
    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
      const XrEventDataSessionStateChanged *session_state_changed_event =
          (XrEventDataSessionStateChanged *)(baseEventHeader);
      ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: "
            "%d for session %p at time %f",
            session_state_changed_event->state,
            (void *)session_state_changed_event->session,
            FromXrTime(session_state_changed_event->time));
      SessionStateChanged(session_state_changed_event->state);

      switch (session_state_changed_event->state) {
      case XR_SESSION_STATE_FOCUSED:
        Focused = true;
        break;
      case XR_SESSION_STATE_VISIBLE:
        Focused = false;
        break;
      case XR_SESSION_STATE_READY:
        HandleSessionStateChanges(session_state_changed_event->state);
        break;
      case XR_SESSION_STATE_STOPPING:
        HandleSessionStateChanges(session_state_changed_event->state);
        break;
      case XR_SESSION_STATE_EXITING:
        ShouldExit = true;
        break;
      default:
        break;
      }
    } break;
    default:
      ALOGV("xrPollEvent: Unknown event");
      break;
    }
  }
}

XrActionSet XrApp::CreateActionSet(uint32_t priority, const char *name,
                                   const char *localizedName) {
  XrActionSetCreateInfo asci = {XR_TYPE_ACTION_SET_CREATE_INFO};
  asci.priority = priority;
  strcpy(asci.actionSetName, name);
  strcpy(asci.localizedActionSetName, localizedName);
  XrActionSet actionSet = XR_NULL_HANDLE;
  OXR(xrCreateActionSet(Instance, &asci, &actionSet));
  return actionSet;
}

XrAction XrApp::CreateAction(XrActionSet actionSet, XrActionType type,
                             const char *actionName, const char *localizedName,
                             int countSubactionPaths, XrPath *subactionPaths) {
  ALOGV("CreateAction %s, %" PRIi32, actionName, countSubactionPaths);

  XrActionCreateInfo aci = {XR_TYPE_ACTION_CREATE_INFO};
  aci.actionType = type;
  if (countSubactionPaths > 0) {
    aci.countSubactionPaths = countSubactionPaths;
    aci.subactionPaths = subactionPaths;
  }
  strcpy(aci.actionName, actionName);
  strcpy(aci.localizedActionName, localizedName ? localizedName : actionName);
  XrAction action = XR_NULL_HANDLE;
  OXR(xrCreateAction(actionSet, &aci, &action));
  return action;
}

XrActionSuggestedBinding
XrApp::ActionSuggestedBinding(XrAction action, const char *bindingString) {
  XrActionSuggestedBinding asb;
  asb.action = action;
  XrPath bindingPath;
  OXR(xrStringToPath(Instance, bindingString, &bindingPath));
  asb.binding = bindingPath;
  return asb;
}

XrSpace XrApp::CreateActionSpace(XrAction poseAction, XrPath subactionPath) {
  XrActionSpaceCreateInfo asci = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
  asci.action = poseAction;
  asci.poseInActionSpace.orientation.w = 1.0f;
  asci.subactionPath = subactionPath;
  XrSpace actionSpace = XR_NULL_HANDLE;
  OXR(xrCreateActionSpace(Session, &asci, &actionSpace));
  return actionSpace;
}

XrActionStateBoolean XrApp::GetActionStateBoolean(XrAction action,
                                                  XrPath subactionPath) {
  XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
  getInfo.action = action;
  getInfo.subactionPath = subactionPath;
  XrActionStateBoolean state = {XR_TYPE_ACTION_STATE_BOOLEAN};
  OXR(xrGetActionStateBoolean(Session, &getInfo, &state));
  return state;
}

XrActionStateFloat XrApp::GetActionStateFloat(XrAction action,
                                              XrPath subactionPath) {
  XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
  getInfo.action = action;
  getInfo.subactionPath = subactionPath;
  XrActionStateFloat state = {XR_TYPE_ACTION_STATE_FLOAT};
  OXR(xrGetActionStateFloat(Session, &getInfo, &state));
  return state;
}

XrActionStateVector2f XrApp::GetActionStateVector2(XrAction action,
                                                   XrPath subactionPath) {
  XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
  getInfo.action = action;
  getInfo.subactionPath = subactionPath;
  XrActionStateVector2f state = {XR_TYPE_ACTION_STATE_VECTOR2F};
  OXR(xrGetActionStateVector2f(Session, &getInfo, &state));
  return state;
}

bool XrApp::ActionPoseIsActive(XrAction action, XrPath subactionPath) {
  XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
  getInfo.action = action;
  getInfo.subactionPath = subactionPath;
  XrActionStatePose state = {XR_TYPE_ACTION_STATE_POSE};
  OXR(xrGetActionStatePose(Session, &getInfo, &state));
  return state.isActive != XR_FALSE;
}

XrApp::LocVel XrApp::GetSpaceLocVel(XrSpace space, XrTime time) {
  XrApp::LocVel lv = {{XR_TYPE_SPACE_LOCATION}, {XR_TYPE_SPACE_VELOCITY}};
  lv.loc.next = &lv.vel;
  OXR(xrLocateSpace(space, CurrentSpace, time, &lv.loc));
  lv.loc.next = NULL; // pointer no longer valid or necessary
  return lv;
}

// Returns a list of OpenXr extensions needed for this app
std::vector<const char *> XrApp::GetExtensions() {
  std::vector<const char *> extensions = {
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
      XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
#elif defined(XR_USE_GRAPHICS_API_OPENGL)
      XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
#endif // defined(XR_USE_GRAPHICS_API_OPENGL_ES)
      XR_KHR_COMPOSITION_LAYER_COLOR_SCALE_BIAS_EXTENSION_NAME,
#if defined(XR_USE_PLATFORM_ANDROID)
      XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME,
      XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME,
#endif // defined(XR_USE_PLATFORM_ANDROID)
      XR_KHR_COMPOSITION_LAYER_CUBE_EXTENSION_NAME,
      XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME};
  return extensions;
}

std::vector<XrExtensionProperties> XrApp::GetXrExtensionProperties() const {
  XrResult result;
  PFN_xrEnumerateInstanceExtensionProperties
      xrEnumerateInstanceExtensionProperties;
  OXR(result = xrGetInstanceProcAddr(
          XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties",
          (PFN_xrVoidFunction *)&xrEnumerateInstanceExtensionProperties));
  if (result != XR_SUCCESS) {
    ALOGE("Failed to get xrEnumerateInstanceExtensionProperties function "
          "pointer.");
    exit(1);
  }

  uint32_t numInputExtensions = 0;
  uint32_t numOutputExtensions = 0;
  OXR(xrEnumerateInstanceExtensionProperties(NULL, numInputExtensions,
                                             &numOutputExtensions, NULL));
  ALOGV("xrEnumerateInstanceExtensionProperties found %u extension(s).",
        numOutputExtensions);

  numInputExtensions = numOutputExtensions;

  std::vector<XrExtensionProperties> extensionProperties(
      numOutputExtensions, {XR_TYPE_EXTENSION_PROPERTIES});

  OXR(xrEnumerateInstanceExtensionProperties(NULL, numInputExtensions,
                                             &numOutputExtensions,
                                             extensionProperties.data()));
  for (uint32_t i = 0; i < numOutputExtensions; i++) {
    ALOGV("Extension #%d = '%s'.", i, extensionProperties[i].extensionName);
  }

  return extensionProperties;
};

// Returns a map from interaction profile paths to vectors of suggested
// bindings. // xrSuggestInteractionProfileBindings() is called once for each
// interaction profile path in the
// // returned map.
// // Apps are encouraged to suggest bindings for every device/interaction
// profile they support.
// Override this for custom action bindings, or modify the default bindings.
std::unordered_map<XrPath, std::vector<XrActionSuggestedBinding>>
XrApp::GetSuggestedBindings(XrInstance instance) {
  if (SkipInputHandling) {
    return std::unordered_map<XrPath, std::vector<XrActionSuggestedBinding>>{};
  }

  std::unordered_map<XrPath, std::vector<XrActionSuggestedBinding>>
      suggestedBindings{};
  // By default we support "oculus/touch_controller" and "khr/simple_controller"
  // as a fallback All supported controllers should be explicitly listed here

  XrPath simpleInteractionProfile = XR_NULL_PATH;
  OXR(xrStringToPath(instance, "/interaction_profiles/khr/simple_controller",
                     &simpleInteractionProfile));

  XrPath touchInteractionProfile = XR_NULL_PATH;
  OXR(xrStringToPath(instance, "/interaction_profiles/oculus/touch_controller",
                     &touchInteractionProfile));

  // -----------------------------------------
  // Bindings for oculus/touch_controller
  // -----------------------------------------
  // Note: using the fact that operator[] creates an object if it doesn't exist
  // in the map
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(AimPoseAction, "/user/hand/left/input/aim/pose"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(AimPoseAction, "/user/hand/right/input/aim/pose"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(GripPoseAction,
                             "/user/hand/left/input/grip/pose"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(GripPoseAction,
                             "/user/hand/right/input/grip/pose"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(JoystickAction,
                             "/user/hand/left/input/thumbstick"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(JoystickAction,
                             "/user/hand/right/input/thumbstick"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(thumbstickClickAction,
                             "/user/hand/left/input/thumbstick/click"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(thumbstickClickAction,
                             "/user/hand/right/input/thumbstick/click"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(IndexTriggerAction,
                             "/user/hand/left/input/trigger/value"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(IndexTriggerAction,
                             "/user/hand/right/input/trigger/value"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(GripTriggerAction,
                             "/user/hand/left/input/squeeze/value"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(GripTriggerAction,
                             "/user/hand/right/input/squeeze/value"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(ButtonAAction, "/user/hand/right/input/a/click"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(ButtonBAction, "/user/hand/right/input/b/click"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(ButtonXAction, "/user/hand/left/input/x/click"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(ButtonYAction, "/user/hand/left/input/y/click"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(ButtonMenuAction,
                             "/user/hand/left/input/menu/click"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(ThumbStickTouchAction,
                             "/user/hand/left/input/thumbstick/touch"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(ThumbStickTouchAction,
                             "/user/hand/right/input/thumbstick/touch"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(ThumbRestTouchAction,
                             "/user/hand/left/input/thumbrest/touch"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(ThumbRestTouchAction,
                             "/user/hand/right/input/thumbrest/touch"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(TriggerTouchAction,
                             "/user/hand/left/input/trigger/touch"));
  suggestedBindings[touchInteractionProfile].emplace_back(
      ActionSuggestedBinding(TriggerTouchAction,
                             "/user/hand/right/input/trigger/touch"));

  // -----------------------------------------
  // Default bindings for khr/simple_controller
  // -----------------------------------------
  suggestedBindings[simpleInteractionProfile].emplace_back(
      ActionSuggestedBinding(AimPoseAction, "/user/hand/left/input/aim/pose"));
  suggestedBindings[simpleInteractionProfile].emplace_back(
      ActionSuggestedBinding(AimPoseAction, "/user/hand/right/input/aim/pose"));
  suggestedBindings[simpleInteractionProfile].emplace_back(
      ActionSuggestedBinding(GripPoseAction,
                             "/user/hand/right/input/grip/pose"));
  suggestedBindings[simpleInteractionProfile].emplace_back(
      ActionSuggestedBinding(GripPoseAction,
                             "/user/hand/left/input/grip/pose"));

  suggestedBindings[simpleInteractionProfile].emplace_back(
      ActionSuggestedBinding(IndexTriggerAction,
                             "/user/hand/right/input/select/click"));
  suggestedBindings[simpleInteractionProfile].emplace_back(
      ActionSuggestedBinding(IndexTriggerAction,
                             "/user/hand/left/input/select/click"));

  suggestedBindings[simpleInteractionProfile].emplace_back(
      ActionSuggestedBinding(ButtonBAction,
                             "/user/hand/right/input/menu/click"));
  suggestedBindings[simpleInteractionProfile].emplace_back(
      ActionSuggestedBinding(ButtonMenuAction,
                             "/user/hand/left/input/menu/click"));

  return suggestedBindings;
}

void XrApp::SuggestInteractionProfileBindings(
    const std::unordered_map<XrPath, std::vector<XrActionSuggestedBinding>>
        allSuggestedBindings) {
  // Best practice is for apps to suggest bindings for *ALL* interaction
  // profiles that the app supports. Loop over all interaction profiles we
  // support and suggest bindings:
  for (auto &[interactionProfilePath, bindings] : allSuggestedBindings) {
    XrInteractionProfileSuggestedBinding suggestedBindings = {
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = interactionProfilePath;
    suggestedBindings.suggestedBindings =
        (const XrActionSuggestedBinding *)bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();

    OXR(xrSuggestInteractionProfileBindings(Instance, &suggestedBindings));
  }
}

const void *XrApp::GetInstanceCreateInfoNextChain() { return nullptr; }

void XrApp::FreeInstanceCreateInfoNextChain(const void *nextChain) {}

const void *XrApp::GetSessionCreateInfoNextChain() { return nullptr; }

void XrApp::FreeSessionCreateInfoNextChain(const void *nextChain) {}

// void XrApp::GetInitialSceneUri(std::string& sceneUri) const {
//     sceneUri = "apk:///assets/box.ovrscene";
// }

XrInstance XrApp::CreateInstance(const xrJava &context) {
#if defined(ANDROID)
  // Loader
  PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
  xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                        (PFN_xrVoidFunction *)&xrInitializeLoaderKHR);
  if (xrInitializeLoaderKHR != NULL) {
    XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid = {
        XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
    loaderInitializeInfoAndroid.applicationVM = context.Vm;
    loaderInitializeInfoAndroid.applicationContext = context.ActivityObject;
    xrInitializeLoaderKHR(
        (XrLoaderInitInfoBaseHeaderKHR *)&loaderInitializeInfoAndroid);
  }
#endif // defined(ANDROID)

  // Log available layers.
  {
    XrResult result;

    PFN_xrEnumerateApiLayerProperties xrEnumerateApiLayerProperties;
    OXR(result = xrGetInstanceProcAddr(
            XR_NULL_HANDLE, "xrEnumerateApiLayerProperties",
            (PFN_xrVoidFunction *)&xrEnumerateApiLayerProperties));
    if (result != XR_SUCCESS) {
      ALOGE("Failed to get xrEnumerateApiLayerProperties function pointer.");
      exit(1);
    }

    uint32_t numInputLayers = 0;
    uint32_t numOutputLayers = 0;
    OXR(xrEnumerateApiLayerProperties(numInputLayers, &numOutputLayers, NULL));

    numInputLayers = numOutputLayers;

    std::vector<XrApiLayerProperties> layerProperties(
        numOutputLayers, {XR_TYPE_API_LAYER_PROPERTIES});

    OXR(xrEnumerateApiLayerProperties(numInputLayers, &numOutputLayers,
                                      layerProperties.data()));

    for (uint32_t i = 0; i < numOutputLayers; i++) {
      ALOGV("Found layer %s", layerProperties[i].layerName);
    }
  }

  // Check that the extensions required are present.
  std::vector<const char *> extensions = GetExtensions();
  ALOGV("Required extension from app (num=%i): ", extensions.size());
  for (auto extension : extensions) {
    ALOGV("\t%s", extension);
  }

  // Check the list of required extensions against what is supported by the
  // runtime. And remove from required list if it is not supported.
  {
    const auto extensionProperties = GetXrExtensionProperties();
    std::vector<std::string> removedExtensions;

    extensions.erase(
        std::remove_if(
            extensions.begin(), extensions.end(),
            [&extensionProperties,
             &removedExtensions](const char *requiredExtensionName) {
              bool found = false;
              for (auto extensionProperty : extensionProperties) {
                if (!strcmp(requiredExtensionName,
                            extensionProperty.extensionName)) {
                  ALOGV("Found required extension %s", requiredExtensionName);
                  found = true;
                  break;
                }
              }

              if (!found) {
                ALOGW("WARNING - Failed to find required extension %s",
                      requiredExtensionName);
                removedExtensions.push_back(requiredExtensionName);
                return true;
              }
              return false;
            }),
        extensions.end());

    if (!removedExtensions.empty()) {
      ALOGW("Following required extensions from app were excluded based on "
            "their existence (num=%i): ",
            removedExtensions.size());
      for (auto extension : removedExtensions) {
        ALOGW("\t%s", extension.c_str());
      }
    }
  }

  // Create the OpenXR instance.
  XrApplicationInfo appInfo;
  memset(&appInfo, 0, sizeof(appInfo));
  strcpy(appInfo.applicationName, "OpenXR_NativeActivity");
  appInfo.applicationVersion = 0;
  strcpy(appInfo.engineName, "Oculus Mobile Sample");
  appInfo.engineVersion = 0;
  appInfo.apiVersion = OpenXRVersion;

  const void *nextChain = GetInstanceCreateInfoNextChain();

  XrInstanceCreateInfo instanceCreateInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
  instanceCreateInfo.next = nextChain;
  instanceCreateInfo.createFlags = 0;
  instanceCreateInfo.applicationInfo = appInfo;
  instanceCreateInfo.enabledApiLayerCount = 0;
  instanceCreateInfo.enabledApiLayerNames = NULL;
  instanceCreateInfo.enabledExtensionCount = extensions.size();
  instanceCreateInfo.enabledExtensionNames = extensions.data();

  XrInstance instance;
  XrResult initResult;
  OXR(initResult = xrCreateInstance(&instanceCreateInfo, &instance));
  if (initResult != XR_SUCCESS) {
    ALOGE("Failed to create XR instance: %d.", initResult);
    exit(1);
  }

  FreeInstanceCreateInfoNextChain(nextChain);

  return instance;
}

// Called one time when the application process starts.
// Returns true if the application initialized successfully.
bool XrApp::Init(const xrJava &context) {
  Instance = CreateInstance(context);
  XrInstanceProperties instanceInfo = {XR_TYPE_INSTANCE_PROPERTIES};
  OXR(xrGetInstanceProperties(Instance, &instanceInfo));
  ALOGV("Runtime %s: Version : %u.%u.%u", instanceInfo.runtimeName,
        XR_VERSION_MAJOR(instanceInfo.runtimeVersion),
        XR_VERSION_MINOR(instanceInfo.runtimeVersion),
        XR_VERSION_PATCH(instanceInfo.runtimeVersion));

  XrSystemGetInfo systemGetInfo = {XR_TYPE_SYSTEM_GET_INFO};
  systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

  XrResult initResult;
  XrSystemId systemId;
  OXR(initResult = xrGetSystem(Instance, &systemGetInfo, &systemId));
  if (initResult != XR_SUCCESS) {
    if (initResult == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
      ALOGE("Failed to get system; the specified form factor is not available. "
            "Is your headset connected?");
    } else {
      ALOGE("xrGetSystem failed, error %d", initResult);
    }
    exit(1);
  }

  XrSystemProperties systemProperties = {XR_TYPE_SYSTEM_PROPERTIES};
  OXR(xrGetSystemProperties(Instance, systemId, &systemProperties));
  ALOGV("System Properties: Name=%s VendorId=%x", systemProperties.systemName,
        systemProperties.vendorId);
  ALOGV("System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
        systemProperties.graphicsProperties.maxSwapchainImageWidth,
        systemProperties.graphicsProperties.maxSwapchainImageHeight,
        systemProperties.graphicsProperties.maxLayerCount);
  ALOGV("System Tracking Properties: OrientationTracking=%s PositionTracking = "
        "% s ",
        systemProperties.trackingProperties.orientationTracking ? "True"
                                                                : "False",
        systemProperties.trackingProperties.positionTracking ? "True"
                                                             : "False");
  assert(MAX_NUM_LAYERS <= systemProperties.graphicsProperties.maxLayerCount);

  // Get the graphics requirements.
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
  PFN_xrGetOpenGLESGraphicsRequirementsKHR
      pfnGetOpenGLESGraphicsRequirementsKHR = NULL;
  OXR(xrGetInstanceProcAddr(
      Instance, "xrGetOpenGLESGraphicsRequirementsKHR",
      (PFN_xrVoidFunction *)(&pfnGetOpenGLESGraphicsRequirementsKHR)));

  XrGraphicsRequirementsOpenGLESKHR graphicsRequirements = {
      XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
  OXR(pfnGetOpenGLESGraphicsRequirementsKHR(Instance, systemId,
                                            &graphicsRequirements));
#elif defined(XR_USE_GRAPHICS_API_OPENGL)
  // Get the graphics requirements.
  PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR =
      NULL;
  OXR(xrGetInstanceProcAddr(
      Instance, "xrGetOpenGLGraphicsRequirementsKHR",
      (PFN_xrVoidFunction *)(&pfnGetOpenGLGraphicsRequirementsKHR)));

  XrGraphicsRequirementsOpenGLKHR graphicsRequirements = {
      XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
  OXR(pfnGetOpenGLGraphicsRequirementsKHR(Instance, systemId,
                                          &graphicsRequirements));
#endif // defined(XR_USE_GRAPHICS_API_OPENGL_ES)

  // Create the EGL Context
  ovrEgl_CreateContext(&Egl, NULL);

  // Check the graphics requirements.
  int eglMajor = 0;
  int eglMinor = 0;
  glGetIntegerv(GL_MAJOR_VERSION, &eglMajor);
  glGetIntegerv(GL_MINOR_VERSION, &eglMinor);
  const XrVersion eglVersion = XR_MAKE_VERSION(eglMajor, eglMinor, 0);
  if (eglVersion < graphicsRequirements.minApiVersionSupported ||
      eglVersion > graphicsRequirements.maxApiVersionSupported) {
    ALOGE("GLES version %d.%d not supported", eglMajor, eglMinor);
    exit(0);
  }
  EglInitExtensions();

  CpuLevel = CPU_LEVEL;
  GpuLevel = GPU_LEVEL;
#if defined(ANDROID)
  MainThreadTid = gettid();
#else
  MainThreadTid = 0;
#endif // defined(ANDROID)
  SystemId = systemId;

  // Actions
  if (!SkipInputHandling) {
    BaseActionSet =
        CreateActionSet(1, "base_action_set", "Action Set used on main loop");

    OXR(xrStringToPath(Instance, "/user/hand/left", &LeftHandPath));
    OXR(xrStringToPath(Instance, "/user/hand/right", &RightHandPath));
    XrPath handSubactionPaths[2] = {LeftHandPath, RightHandPath};

    AimPoseAction = CreateAction(BaseActionSet, XR_ACTION_TYPE_POSE_INPUT,
                                 "aim_pose", NULL, 2, handSubactionPaths);
    GripPoseAction = CreateAction(BaseActionSet, XR_ACTION_TYPE_POSE_INPUT,
                                  "grip_pose", NULL, 2, handSubactionPaths);

    JoystickAction = CreateAction(BaseActionSet, XR_ACTION_TYPE_VECTOR2F_INPUT,
                                  "move_on_joy", NULL, 2, handSubactionPaths);

    IndexTriggerAction =
        CreateAction(BaseActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "index_trigger",
                     NULL, 2, handSubactionPaths);

    GripTriggerAction =
        CreateAction(BaseActionSet, XR_ACTION_TYPE_FLOAT_INPUT, "grip_trigger",
                     NULL, 2, handSubactionPaths);
    ButtonAAction = CreateAction(BaseActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT,
                                 "button_a", NULL, 2, handSubactionPaths);
    ButtonBAction = CreateAction(BaseActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT,
                                 "button_b", NULL, 2, handSubactionPaths);
    ButtonXAction = CreateAction(BaseActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT,
                                 "button_x", NULL, 2, handSubactionPaths);
    ButtonYAction = CreateAction(BaseActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT,
                                 "button_y", NULL, 2, handSubactionPaths);
    ButtonMenuAction = CreateAction(BaseActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT,
                                    "button_menu", NULL, 2, handSubactionPaths);
    ThumbStickTouchAction =
        CreateAction(BaseActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT,
                     "thumb_stick_touch", NULL, 2, handSubactionPaths);
    ThumbRestTouchAction =
        CreateAction(BaseActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT,
                     "thumb_rest_touch", NULL, 2, handSubactionPaths);
    TriggerTouchAction =
        CreateAction(BaseActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT,
                     "index_trigger_touch", NULL, 2, handSubactionPaths);

    thumbstickClickAction =
        CreateAction(BaseActionSet, XR_ACTION_TYPE_BOOLEAN_INPUT,
                     "thumbstick_click", NULL, 2, handSubactionPaths);
  }

  /// Interaction profile can be overridden
  const std::unordered_map<XrPath, std::vector<XrActionSuggestedBinding>>
      allSuggestedBindings = GetSuggestedBindings(GetInstance());

  SuggestInteractionProfileBindings(allSuggestedBindings);

  // FileSys = std::unique_ptr<OVRFW::ovrFileSys>(ovrFileSys::Create(context));
  // if (FileSys) {
  //   OVRFW::ovrFileSys &fs = *FileSys;
  //   MaterialParms materialParms;
  //   materialParms.UseSrgbTextureFormats = false;
  //   std::string sceneUri;
  //   GetInitialSceneUri(sceneUri);
  //   if (!sceneUri.empty()) {
  //     SceneModel = std::unique_ptr<OVRFW::ModelFile>(LoadModelFile(
  //         fs, sceneUri.c_str(), Scene.GetDefaultGLPrograms(),
  //         materialParms));
  //     if (SceneModel != nullptr) {
  //       Scene.SetWorldModel(*SceneModel);
  //     }
  //   }
  // }
  SurfaceRender.Init();

  return AppInit(&context);
}

bool XrApp::InitSession() {
  // Create the OpenXR Session.
  const void *nextChain = GetSessionCreateInfoNextChain();

#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
  XrGraphicsBindingOpenGLESAndroidKHR graphicsBindingAndroidGLES = {
      XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
  graphicsBindingAndroidGLES.next = nextChain;
  graphicsBindingAndroidGLES.display = Egl.Display;
  graphicsBindingAndroidGLES.config = Egl.Config;
  graphicsBindingAndroidGLES.context = Egl.Context;
#elif defined(XR_USE_GRAPHICS_API_OPENGL)
  XrGraphicsBindingOpenGLWin32KHR graphicsBindingGL = {
      XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
  graphicsBindingGL.next = nextChain;
  graphicsBindingGL.hDC = Egl.hDC;
  graphicsBindingGL.hGLRC = Egl.hGLRC;
#endif // defined(XR_USE_GRAPHICS_API_OPENGL_ES)

  XrSessionCreateInfo sessionCreateInfo = {XR_TYPE_SESSION_CREATE_INFO};
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
  sessionCreateInfo.next = &graphicsBindingAndroidGLES;
#elif defined(XR_USE_GRAPHICS_API_OPENGL)
  sessionCreateInfo.next = &graphicsBindingGL;
#endif
  sessionCreateInfo.createFlags = 0;
  sessionCreateInfo.systemId = SystemId;

  PreCreateSession(sessionCreateInfo);
  XrResult initResult;
  OXR(initResult = xrCreateSession(Instance, &sessionCreateInfo, &Session));
  if (initResult != XR_SUCCESS) {
    ALOGE("Failed to create XR session: %d.", initResult);
    exit(1);
  }
  PostCreateSession(sessionCreateInfo);

  FreeSessionCreateInfoNextChain(nextChain);

  // App only supports the primary stereo view config.
  const XrViewConfigurationType supportedViewConfigType =
      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

  // Enumerate the viewport configurations.
  {
    uint32_t viewportConfigTypeCount = 0;
    OXR(xrEnumerateViewConfigurations(Instance, SystemId, 0,
                                      &viewportConfigTypeCount, NULL));

    std::vector<XrViewConfigurationType> viewportConfigurationTypes(
        viewportConfigTypeCount);

    OXR(xrEnumerateViewConfigurations(
        Instance, SystemId, viewportConfigTypeCount, &viewportConfigTypeCount,
        viewportConfigurationTypes.data()));

    ALOGV("Available Viewport Configuration Types: %d",
          viewportConfigTypeCount);
    for (uint32_t i = 0; i < viewportConfigTypeCount; i++) {
      const XrViewConfigurationType viewportConfigType =
          viewportConfigurationTypes[i];
      ALOGV("Viewport configuration type %d : %s", viewportConfigType,
            viewportConfigType == supportedViewConfigType ? "Selected" : "");
      XrViewConfigurationProperties viewportConfig{
          XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
      OXR(xrGetViewConfigurationProperties(
          Instance, SystemId, viewportConfigType, &viewportConfig));
      ALOGV("FovMutable=%s ConfigurationType %d",
            viewportConfig.fovMutable ? "true" : "false",
            viewportConfig.viewConfigurationType);

      uint32_t viewCount;
      OXR(xrEnumerateViewConfigurationViews(
          Instance, SystemId, viewportConfigType, 0, &viewCount, NULL));

      if (viewCount > 0) {
        std::vector<XrViewConfigurationView> elements(
            viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});

        OXR(xrEnumerateViewConfigurationViews(Instance, SystemId,
                                              viewportConfigType, viewCount,
                                              &viewCount, elements.data()));

        // Log the view config info for each view type for debugging purposes.
        for (uint32_t e = 0; e < viewCount; e++) {
          const XrViewConfigurationView *element = &elements[e];

          ALOGV("Viewport [%d]: Recommended Width=%d Height=%d SampleCount = % "
                "d ",
                e, element->recommendedImageRectWidth,
                element->recommendedImageRectHeight,
                element->recommendedSwapchainSampleCount);

          ALOGV("Viewport [%d]: Max Width=%d Height=%d SampleCount = % d ", e,
                element->maxImageRectWidth, element->maxImageRectHeight,
                element->maxSwapchainSampleCount);
        }

        // Cache the view config properties for the selected config type.
        if (viewportConfigType == supportedViewConfigType) {
          assert(viewCount == MAX_NUM_EYES);
          for (uint32_t e = 0; e < viewCount; e++) {
            ViewConfigurationView[e] = elements[e];
          }
        }
      } else {
        ALOGE("Empty viewport configuration type: %d", viewCount);
      }
    }
  }

  // Get the viewport configuration info for the chosen viewport configuration
  // type.
  OXR(xrGetViewConfigurationProperties(
      Instance, SystemId, supportedViewConfigType, &ViewportConfig));

  for (int eye = 0; eye < MAX_NUM_EYES; eye++) {
    Projections[eye] = XrView{XR_TYPE_VIEW};
  }

  bool stageSupported = false;
  uint32_t numOutputSpaces = 0;
  OXR(xrEnumerateReferenceSpaces(Session, 0, &numOutputSpaces, NULL));

  std::vector<XrReferenceSpaceType> referenceSpaces(numOutputSpaces);
  OXR(xrEnumerateReferenceSpaces(Session, numOutputSpaces, &numOutputSpaces,
                                 referenceSpaces.data()));
  for (uint32_t i = 0; i < numOutputSpaces; i++) {
    if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE) {
      stageSupported = true;
      break;
    }
  }

  // Create a space to the first path
  XrReferenceSpaceCreateInfo spaceCreateInfo = {
      XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
  OXR(xrCreateReferenceSpace(Session, &spaceCreateInfo, &HeadSpace));

  spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  OXR(xrCreateReferenceSpace(Session, &spaceCreateInfo, &LocalSpace));

  // xrCreateActionSpace requires Session, create them here.
  LeftControllerAimSpace = CreateActionSpace(AimPoseAction, LeftHandPath);
  RightControllerAimSpace = CreateActionSpace(AimPoseAction, RightHandPath);
  LeftControllerGripSpace = CreateActionSpace(GripPoseAction, LeftHandPath);
  RightControllerGripSpace = CreateActionSpace(GripPoseAction, RightHandPath);

  // to use as fake stage
  if (stageSupported) {
    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    spaceCreateInfo.poseInReferenceSpace.position.y = 0.0f;
    OXR(xrCreateReferenceSpace(Session, &spaceCreateInfo, &StageSpace));
    ALOGV("Created stage space");
    CurrentSpace = StageSpace;
  }

  // Create the frame buffers.
  for (int eye = 0; eye < MAX_NUM_EYES; eye++) {
    ovrFramebuffer_Create(Session, &FrameBuffer[eye], GL_SRGB8_ALPHA8,
                          ViewConfigurationView[0].recommendedImageRectWidth *
                              FramebufferResolutionScaleFactor,
                          ViewConfigurationView[0].recommendedImageRectHeight *
                              FramebufferResolutionScaleFactor,
                          NUM_MULTI_SAMPLES);
  }

  // xrAttachSessionActionSets can only be called once, so skip it if the
  // application is doing it manually
  if (!SkipInputHandling) {
    // Attach to session
    AttachActionSets();
  }

  return SessionInit();
}

void XrApp::EndSession() {
  for (int eye = 0; eye < MAX_NUM_EYES; eye++) {
    ovrFramebuffer_Destroy(&FrameBuffer[eye]);
  }

  OXR(xrDestroySpace(HeadSpace));
  OXR(xrDestroySpace(LocalSpace));
  // StageSpace is optional.
  if (StageSpace != XR_NULL_HANDLE) {
    OXR(xrDestroySpace(StageSpace));
  }
  CurrentSpace = XR_NULL_HANDLE;
  SessionEnd();
  OXR(xrDestroySession(Session));

  ovrEgl_DestroyContext(&Egl);
}

void XrApp::DestroyInstance() { OXR(xrDestroyInstance(Instance)); }

// Called one time when the applicatoin process exits
void XrApp::Shutdown(const xrJava &context) {
  AppShutdown(&context);
  DestroyInstance();
  Clear();
}

// Called on each Shutdown, reset all member variable to initial state
void XrApp::Clear() {
#if defined(ANDROID)
  Resumed = false;
#endif // defined(ANDROID)
  ShouldExit = false;
  Focused = false;
  SkipInputHandling = false;

  Instance = XR_NULL_HANDLE;
  Session = XR_NULL_HANDLE;
  ViewportConfig = {};
  for (int i = 0; i < MAX_NUM_EYES; i++) {
    ViewConfigurationView[i] = {};
  }
  for (int i = 0; i < MAX_NUM_EYES; i++) {
    Projections[i] = {};
  }
  SystemId = XR_NULL_SYSTEM_ID;
  HeadSpace = XR_NULL_HANDLE;
  LocalSpace = XR_NULL_HANDLE;
  StageSpace = XR_NULL_HANDLE;
  CurrentSpace = XR_NULL_HANDLE;
  SessionActive = false;

  BaseActionSet = XR_NULL_HANDLE;
  LeftHandPath = XR_NULL_PATH;
  RightHandPath = XR_NULL_PATH;
  AimPoseAction = XR_NULL_HANDLE;
  GripPoseAction = XR_NULL_HANDLE;
  JoystickAction = XR_NULL_HANDLE;
  thumbstickClickAction = XR_NULL_HANDLE;
  IndexTriggerAction = XR_NULL_HANDLE;
  IndexTriggerClickAction = XR_NULL_HANDLE;
  GripTriggerAction = XR_NULL_HANDLE;
  ButtonAAction = XR_NULL_HANDLE;
  ButtonBAction = XR_NULL_HANDLE;
  ButtonXAction = XR_NULL_HANDLE;
  ButtonYAction = XR_NULL_HANDLE;
  ButtonMenuAction = XR_NULL_HANDLE;
  /// common touch actions
  ThumbStickTouchAction = XR_NULL_HANDLE;
  ThumbRestTouchAction = XR_NULL_HANDLE;
  TriggerTouchAction = XR_NULL_HANDLE;

  LeftControllerAimSpace = XR_NULL_HANDLE;
  RightControllerAimSpace = XR_NULL_HANDLE;
  LeftControllerGripSpace = XR_NULL_HANDLE;
  RightControllerGripSpace = XR_NULL_HANDLE;
  LastFrameAllButtons = 0u;
  LastFrameAllTouches = 0u;
}

// Internal Input
void XrApp::AttachActionSets() {
  XrSessionActionSetsAttachInfo attachInfo = {
      XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attachInfo.countActionSets = 1;
  attachInfo.actionSets = &BaseActionSet;
  OXR(xrAttachSessionActionSets(Session, &attachInfo));
}

void XrApp::SyncActionSets(ovrApplFrameIn &in) {
  // sync action data
  XrActiveActionSet activeActionSet{BaseActionSet};
  XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
  syncInfo.countActiveActionSets = 1;
  syncInfo.activeActionSets = &activeActionSet;
  OXR(xrSyncActions(Session, &syncInfo));

  // query input action states
  XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
  getInfo.subactionPath = XR_NULL_PATH;

  XrAction controller[] = {AimPoseAction, GripPoseAction, AimPoseAction,
                           GripPoseAction};
  XrPath subactionPath[] = {LeftHandPath, LeftHandPath, RightHandPath,
                            RightHandPath};
  XrSpace controllerSpace[] = {
      LeftControllerAimSpace,
      LeftControllerGripSpace,
      RightControllerAimSpace,
      RightControllerGripSpace,
  };
  bool ControllerPoseActive[] = {false, false, false, false};
  XrPosef ControllerPose[] = {{}, {}, {}, {}};
  for (int i = 0; i < 4; i++) {
    if (ActionPoseIsActive(controller[i], subactionPath[i])) {
      LocVel lv =
          GetSpaceLocVel(controllerSpace[i], ToXrTime(in.PredictedDisplayTime));
      ControllerPoseActive[i] =
          (lv.loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
      ControllerPose[i] = lv.loc.pose;
    } else {
      ControllerPoseActive[i] = false;
      XrPosef_CreateIdentity(&ControllerPose[i]);
    }
  }

#if 0 // Enable for debugging current interaction profile issues
    XrPath topLevelUserPath = XR_NULL_PATH;
    OXR(xrStringToPath(Instance, "/user/hand/left", &topLevelUserPath));
    XrInteractionProfileState ipState{XR_TYPE_INTERACTION_PROFILE_STATE};
    xrGetCurrentInteractionProfile(Session, topLevelUserPath, &ipState);
    uint32_t ipPathOutSize = 0;
    char ipPath[256];
    xrPathToString(Instance, ipState.interactionProfile, sizeof(ipPath),
    &ipPathOutSize, ipPath); ALOG( "Current interaction profile is: '%s'",
    ipPath);
#endif

  /// Update pose
  XrSpaceLocation loc = {XR_TYPE_SPACE_LOCATION};
  OXR(xrLocateSpace(HeadSpace, CurrentSpace, ToXrTime(in.PredictedDisplayTime),
                    &loc));
  in.HeadPose = FromXrPosef(loc.pose);
  /// grip & point space
  in.LeftRemotePointPose = FromXrPosef(ControllerPose[0]);
  in.LeftRemotePose = FromXrPosef(ControllerPose[1]);
  in.RightRemotePointPose = FromXrPosef(ControllerPose[2]);
  in.RightRemotePose = FromXrPosef(ControllerPose[3]);
  in.LeftRemoteTracked = ControllerPoseActive[1];
  in.RightRemoteTracked = ControllerPoseActive[3];

  in.LeftRemoteIndexTrigger =
      GetActionStateFloat(IndexTriggerAction, LeftHandPath).currentState;
  in.RightRemoteIndexTrigger =
      GetActionStateFloat(IndexTriggerAction, RightHandPath).currentState;
  in.LeftRemoteGripTrigger =
      GetActionStateFloat(GripTriggerAction, LeftHandPath).currentState;
  in.RightRemoteGripTrigger =
      GetActionStateFloat(GripTriggerAction, RightHandPath).currentState;
  in.LeftRemoteJoystick = FromXrVector2f(
      GetActionStateVector2(JoystickAction, LeftHandPath).currentState);
  in.RightRemoteJoystick = FromXrVector2f(
      GetActionStateVector2(JoystickAction, RightHandPath).currentState);

  bool aPressed = GetActionStateBoolean(ButtonAAction).currentState;
  bool bPressed = GetActionStateBoolean(ButtonBAction).currentState;
  bool xPressed = GetActionStateBoolean(ButtonXAction).currentState;
  bool yPressed = GetActionStateBoolean(ButtonYAction).currentState;
  bool menuPressed = GetActionStateBoolean(ButtonMenuAction).currentState;
  bool leftThumbPressed =
      GetActionStateBoolean(thumbstickClickAction, LeftHandPath).currentState;
  bool rightThumbPressed =
      GetActionStateBoolean(thumbstickClickAction, RightHandPath).currentState;

  in.LastFrameAllButtons = LastFrameAllButtons;
  in.AllButtons = 0u;

  if (aPressed) {
    in.AllButtons |= ovrApplFrameIn::kButtonA;
  }
  if (bPressed) {
    in.AllButtons |= ovrApplFrameIn::kButtonB;
  }
  if (xPressed) {
    in.AllButtons |= ovrApplFrameIn::kButtonX;
  }
  if (yPressed) {
    in.AllButtons |= ovrApplFrameIn::kButtonY;
  }
  if (menuPressed) {
    in.AllButtons |= ovrApplFrameIn::kButtonMenu;
  }
  if (leftThumbPressed) {
    in.AllButtons |= ovrApplFrameIn::kButtonLeftThumbStick;
  }
  if (rightThumbPressed) {
    in.AllButtons |= ovrApplFrameIn::kButtonRightThumbStick;
  }
  if (in.LeftRemoteIndexTrigger > 0.1f) {
    in.AllButtons |= ovrApplFrameIn::kTrigger;
  }
  if (in.RightRemoteIndexTrigger > 0.1f) {
    in.AllButtons |= ovrApplFrameIn::kTrigger;
  }
  if (in.LeftRemoteGripTrigger > 0.1f) {
    in.AllButtons |= ovrApplFrameIn::kGripTrigger;
  }
  if (in.RightRemoteGripTrigger > 0.1f) {
    in.AllButtons |= ovrApplFrameIn::kGripTrigger;
  }
  LastFrameAllButtons = in.AllButtons;

  /// touch
  in.LastFrameAllTouches = LastFrameAllTouches;
  in.AllTouches = 0u;

  const bool thumbstickTouched =
      GetActionStateBoolean(ThumbStickTouchAction).currentState;
  const bool thumbrestTouched =
      GetActionStateBoolean(ThumbRestTouchAction).currentState;
  const bool triggerTouched =
      GetActionStateBoolean(TriggerTouchAction).currentState;

  if (thumbstickTouched) {
    in.AllTouches |= ovrApplFrameIn::kTouchJoystick;
  }
  if (thumbrestTouched) {
    in.AllTouches |= ovrApplFrameIn::kTouchThumbrest;
  }
  if (triggerTouched) {
    in.AllTouches |= ovrApplFrameIn::kTouchTrigger;
  }

  LastFrameAllTouches = in.AllTouches;

  /*
      /// timing
      double RealTimeInSeconds = 0.0;
      float DeltaSeconds = 0.0f;
      /// device config
      float IPD = 0.065f;
      float EyeHeight = 1.6750f;
      int32_t RecenterCount = 0;
  */
}

void XrApp::HandleInput(ovrApplFrameIn &in) {
  if (!SkipInputHandling) {
    // Sync default actions
    SyncActionSets(in);
  }

  // Call application Update function
  Update(in);
}

// Called once per frame to allow the application to render eye buffers.
void XrApp::AppRenderFrame(const OVRFW::ovrApplFrameIn &in,
                           OVRFW::ovrRendererOutput &out) {
  // Scene.SetFreeMove(FreeMove);
  /// create a local copy
  OVRFW::ovrApplFrameIn localIn = in;
  if (false == FreeMove) {
    localIn.LeftRemoteJoystick.x = 0.0f;
    localIn.LeftRemoteJoystick.y = 0.0f;
    localIn.RightRemoteJoystick.x = 0.0f;
    localIn.RightRemoteJoystick.y = 0.0f;
  }
  // Scene.Frame(localIn);
  // Scene.GenerateFrameSurfaceList(out.FrameMatrices, out.Surfaces);
  if (ShouldRender) {
    Render(in, out);
  }

  for (int eye = 0; eye < MAX_NUM_EYES; eye++) {
    ovrFramebuffer *frameBuffer = &FrameBuffer[eye];
    ovrFramebuffer_Acquire(frameBuffer);
    ovrFramebuffer_SetCurrent(frameBuffer);

    AppEyeGLStateSetup(in, frameBuffer, eye);
    AppRenderEye(in, out, eye);

    ovrFramebuffer_Resolve(frameBuffer);
    ovrFramebuffer_Release(frameBuffer);
  }
  ovrFramebuffer_SetNone();
}

void XrApp::AppRenderEye(const OVRFW::ovrApplFrameIn &in,
                         OVRFW::ovrRendererOutput &out, int eye) {
  // Render the surfaces returned by Frame.
  SurfaceRender.RenderSurfaceList(
      out.Surfaces,
      out.FrameMatrices.EyeView[0],       // always use 0 as it assumes an array
      out.FrameMatrices.EyeProjection[0], // always use 0 as it assumes an array
      eye);
}

// Called once per eye each frame for default renderer
void XrApp::AppEyeGLStateSetup(const ovrApplFrameIn &in,
                               const ovrFramebuffer *fb, int eye) {
  GL(glEnable(GL_SCISSOR_TEST));
  GL(glDepthMask(GL_TRUE));
  GL(glEnable(GL_DEPTH_TEST));
  GL(glDepthFunc(GL_LEQUAL));
  GL(glEnable(GL_CULL_FACE));
  GL(glViewport(0, 0, fb->Width, fb->Height));
  GL(glScissor(0, 0, fb->Width, fb->Height));
  GL(glClearColor(BackgroundColor.x, BackgroundColor.y, BackgroundColor.z,
                  BackgroundColor.w));
  GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

// Called when the application initializes.
// Overridden by the actual app
// Must return true if the application initializes successfully.
bool XrApp::AppInit(const xrJava *context) { return true; }

// Called when the application shuts down
void XrApp::AppShutdown(const xrJava *context) { SurfaceRender.Shutdown(); }

// // Called when the application is resumed by the system.
// void XrApp::AppResumed(const xrJava* contet) {}

// // Called when the application is paused by the system.
// void XrApp::AppPaused(const xrJava* context) {}

// // Called when app loses focus
// void XrApp::AppLostFocus() {}

// // Called when app re-gains focus
// void XrApp::AppGainedFocus() {}

bool XrApp::SessionInit() { return true; }

void XrApp::SessionEnd() {}

void XrApp::SessionStateChanged(XrSessionState) {}

void XrApp::Update(const ovrApplFrameIn &in) {}

void XrApp::Render(const ovrApplFrameIn &in, ovrRendererOutput &out) {}

// #if defined(ANDROID)
// void ActivityMainLoopContext::HandleOsEvents() {
//     // Read all pending events.
//     for (;;) {
//         int events;
//         struct android_poll_source* source;
//         // If the timeout is zero, returns immediately without blocking.
//         // If the timeout is negative, waits indefinitely until an event
//         appears. const int timeoutMilliseconds =
//             (xrApp_->GetResumed() == false && xrApp_->GetSessionActive() ==
//             false &&
//              app_->destroyRequested == 0)
//             ? -1
//             : 0;
//         if (ALooper_pollAll(timeoutMilliseconds, NULL, &events,
//         (void**)&source) < 0) {
//             break;
//         }
//
//         // Process this event.
//         if (source != NULL) {
//             source->process(app_, source);
//         }
//     }
// }

// bool ActivityMainLoopContext::ShouldExitMainLoop() const {
//     return app_->destroyRequested;
// }
//
// bool ActivityMainLoopContext::IsExitRequested() const {
//     return xrApp_->GetShouldExit();
// }
// #elif defined(WIN32)
void WindowsMainLoopContext::HandleOsEvents() {
  MSG msg;
  while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
    if (msg.message == WM_QUIT) {
      xrApp_->SetShouldExit(true);
    } else {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  }
}

bool WindowsMainLoopContext::ShouldExitMainLoop() const { return false; }

bool WindowsMainLoopContext::IsExitRequested() const {
  return xrApp_->GetShouldExit();
}
// #else
// #error "Platform not supported!"
// #endif // defined(ANDROID)

// Main application loop. The MainLoopContext is a functor that allows
// an application to overload exit condition and event polling within the
// loop. This allows Android Activity-based apps, Android Service-based apps,
// and Windows apps to all use the same thread loop.
void XrApp::MainLoop(MainLoopContext &loopContext) {
  if (!Init(loopContext.GetJavaContext())) {
    ALOGE("Application failed to initialize.");
    return;
  }

  InitSession();

  bool stageBoundsDirty = true;
  int frameCount = -1;

  while (!loopContext.ShouldExitMainLoop()) {
    frameCount++;

    loopContext.HandleOsEvents();

    HandleXrEvents();

    if (loopContext.IsExitRequested()) {
      break;
    }

    if (SessionActive == false) {
      continue;
    }

    if (stageBoundsDirty) {
      XrExtent2Df stageBounds = {};
      XrResult result;
      OXR(result = xrGetReferenceSpaceBoundsRect(
              Session, XR_REFERENCE_SPACE_TYPE_STAGE, &stageBounds));
      stageBoundsDirty = false;
    }

    // NOTE: OpenXR does not use the concept of frame indices. Instead,
    // XrWaitFrame returns the predicted display time.
    XrFrameWaitInfo waitFrameInfo = {XR_TYPE_FRAME_WAIT_INFO};

    PreWaitFrame(waitFrameInfo);

    XrFrameState frameState = {XR_TYPE_FRAME_STATE};

    OXR(xrWaitFrame(Session, &waitFrameInfo, &frameState));

    // Get the HMD pose, predicted for the middle of the time period during
    // which the new eye images will be displayed. The number of frames
    // predicted ahead depends on the pipeline depth of the engine and the
    // synthesis rate.
    // The better the prediction, the less black will be pulled in at the
    // edges.
    XrFrameBeginInfo beginFrameDesc = {XR_TYPE_FRAME_BEGIN_INFO};
    OXR(xrBeginFrame(Session, &beginFrameDesc));
    ShouldRender = frameState.shouldRender;

    XrSpaceLocation loc = {XR_TYPE_SPACE_LOCATION};
    OXR(xrLocateSpace(HeadSpace, CurrentSpace, frameState.predictedDisplayTime,
                      &loc));
    XrPosef xfStageFromHead = loc.pose;
    OXR(xrLocateSpace(HeadSpace, LocalSpace, frameState.predictedDisplayTime,
                      &loc));

    XrViewState viewState = {XR_TYPE_VIEW_STATE};

    XrViewLocateInfo projectionInfo = {XR_TYPE_VIEW_LOCATE_INFO};
    projectionInfo.viewConfigurationType = ViewportConfig.viewConfigurationType;
    projectionInfo.displayTime = frameState.predictedDisplayTime;
    projectionInfo.space = HeadSpace;

    uint32_t projectionCapacityInput = MAX_NUM_EYES;
    uint32_t projectionCountOutput = projectionCapacityInput;

    PreLocateViews(projectionInfo);
    OXR(xrLocateViews(Session, &projectionInfo, &viewState,
                      projectionCapacityInput, &projectionCountOutput,
                      Projections));

    OVRFW::ovrApplFrameIn in = {};
    OVRFW::ovrRendererOutput out = {};
    in.FrameIndex = frameCount;

    /// time accounting
    in.PredictedDisplayTime = FromXrTime(frameState.predictedDisplayTime);
    if (PrevDisplayTime > 0) {
      in.DeltaSeconds =
          FromXrTime(frameState.predictedDisplayTime - PrevDisplayTime);
    }
    PrevDisplayTime = frameState.predictedDisplayTime;

    for (int eye = 0; eye < MAX_NUM_EYES; eye++) {
      XrPosef xfHeadFromEye = Projections[eye].pose;
      XrPosef xfStageFromEye{};
      XrPosef_Multiply(&xfStageFromEye, &xfStageFromHead, &xfHeadFromEye);
      XrPosef_Invert(&ViewTransform[eye], &xfStageFromEye);
      XrMatrix4x4f viewMat{};
      XrMatrix4x4f_CreateFromRigidTransform(&viewMat, &ViewTransform[eye]);
      const XrFovf fov = Projections[eye].fov;
      XrMatrix4x4f projMat;
      XrMatrix4x4f_CreateProjectionFov(&projMat, GRAPHICS_OPENGL_ES, fov, 0.1f,
                                       0.0f);
      out.FrameMatrices.EyeView[eye] = FromXrMatrix4x4f(viewMat);
      out.FrameMatrices.EyeProjection[eye] = FromXrMatrix4x4f(projMat);
      in.Eye[eye].ViewMatrix = out.FrameMatrices.EyeView[eye];
      in.Eye[eye].ProjectionMatrix = out.FrameMatrices.EyeProjection[eye];
    }

    XrPosef centerView;
    XrPosef_Invert(&centerView, &xfStageFromHead);
    XrMatrix4x4f viewMat{};
    XrMatrix4x4f_CreateFromRigidTransform(&viewMat, &centerView);
    out.FrameMatrices.CenterView = FromXrMatrix4x4f(viewMat);

    // Input
    HandleInput(in);

    LayerCount = 0;
    memset(Layers, 0, sizeof(xrCompositorLayerUnion) * MAX_NUM_LAYERS);

    // allow apps to submit a layer before the world view projection
    // layer(uncommon)
    PreProjectionAddLayer(Layers, LayerCount);

    // Render the world-view layer (projection)
    AppRenderFrame(in, out);
    ProjectionAddLayer(Layers, LayerCount);

    // allow apps to submit a layer after the world view projection layer
    // (uncommon)
    PostProjectionAddLayer(Layers, LayerCount);

    // Compose the layers for this frame.
    const XrCompositionLayerBaseHeader *layers[MAX_NUM_LAYERS] = {};
    for (int i = 0; i < LayerCount; i++) {
      layers[i] = (const XrCompositionLayerBaseHeader *)&Layers[i];
    }

    XrFrameEndInfo endFrameInfo = {XR_TYPE_FRAME_END_INFO};
    endFrameInfo.displayTime = frameState.predictedDisplayTime;
    endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endFrameInfo.layerCount = LayerCount;
    endFrameInfo.layers = layers;

    OXR(xrEndFrame(Session, &endFrameInfo));
  }

  EndSession();
  Shutdown(loopContext.GetJavaContext());
}

void XrApp::ProjectionAddLayer(xrCompositorLayerUnion *layers,
                               int &layerCount) {
  // Set-up the compositor layers for this frame.
  // NOTE: Multiple independent layers are allowed, but they need to be added
  // in a depth consistent order.
  XrCompositionLayerProjection projection_layer = {
      XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  projection_layer.layerFlags =
      XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
  projection_layer.layerFlags |=
      XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
  projection_layer.space = CurrentSpace;
  projection_layer.viewCount = MAX_NUM_EYES;
  projection_layer.views = ProjectionLayerElements;

  for (int eye = 0; eye < MAX_NUM_EYES; eye++) {
    ovrFramebuffer *frameBuffer = &FrameBuffer[eye];
    memset(&ProjectionLayerElements[eye], 0,
           sizeof(XrCompositionLayerProjectionView));
    ProjectionLayerElements[eye].type =
        XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
    XrPosef_Invert(&ProjectionLayerElements[eye].pose, &ViewTransform[eye]);
    ProjectionLayerElements[eye].fov = Projections[eye].fov;
    memset(&ProjectionLayerElements[eye].subImage, 0,
           sizeof(XrSwapchainSubImage));
    ProjectionLayerElements[eye].subImage.swapchain =
        frameBuffer->ColorSwapChain.Handle;
    ProjectionLayerElements[eye].subImage.imageRect.offset.x = 0;
    ProjectionLayerElements[eye].subImage.imageRect.offset.y = 0;
    ProjectionLayerElements[eye].subImage.imageRect.extent.width =
        frameBuffer->ColorSwapChain.Width;
    ProjectionLayerElements[eye].subImage.imageRect.extent.height =
        frameBuffer->ColorSwapChain.Height;
    ProjectionLayerElements[eye].subImage.imageArrayIndex = 0;
  }

  layers[layerCount++].Projection = projection_layer;
}

// App entry point
#if defined(ANDROID)
void XrApp::Run(struct android_app *app) {
  // Setup Activity-specific state
  ALOGV("----------------------------------------------------------------");
  ALOGV("android_app_entry()");
  ALOGV("    android_main()");

  // TODO: We should make this not required for OOPC apps.
  ANativeActivity_setWindowFlags(app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0);

  JNIEnv *Env;
  (*app->activity->vm).AttachCurrentThread(&Env, nullptr);

  // Note that AttachCurrentThread will reset the thread name.
  prctl(PR_SET_NAME, (long)"XrApp::Main", 0, 0, 0);

  Context.Vm = app->activity->vm;
  Context.Env = Env;
  Context.ActivityObject = app->activity->clazz;

  app->userData = this;
  app->onAppCmd = app_handle_cmd;

  ActivityMainLoopContext loopContext(Context, this, app);
#elif defined(WIN32)
void XrApp::Run() {
  // Context.Vm = nullptr;
  // Context.Env = nullptr;
  // Context.ActivityObject = nullptr;

  WindowsMainLoopContext loopContext(Context, this);
#else
#error "Platform not supported!"
#endif // defined(ANDROID)

  MainLoop(loopContext);

#if defined(ANDROID)
  (*app->activity->vm).DetachCurrentThread();
#endif // defined(ANDROID)
}

} // namespace OVRFW

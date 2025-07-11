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

Filename    :   XrApp.h
Content     :   OpenXR application base class.
Created     :   July 2020
Authors     :   Federico Schliemann
Language    :   c++

*******************************************************************************/

#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

// #include "OVR_Math.h"

// #include "System.h"
#include "FrameParams.h"
// #include "OVR_FileSys.h"

#include "Misc/Log.h"
#include "Render/Egl.h"

#if defined(ANDROID)
#include <android/keycodes.h>
#include <android/native_window_jni.h>
#include <android/window.h>
#include <android_native_app_glue.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif
#elif defined(WIN32)
#include "windows.h"

#endif // defined(ANDROID)

// EXT_texture_border_clamp
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif

// #if !defined(GL_EXT_multisampled_render_to_texture)
// typedef void(GL_APIENTRY* PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)(
//     GLenum target,
//     GLsizei samples,
//     GLenum internalformat,
//     GLsizei width,
//     GLsizei height);
// typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)(
//     GLenum target,
//     GLenum attachment,
//     GLenum textarget,
//     GLuint texture,
//     GLint level,
//     GLsizei samples);
// #endif

// GL_EXT_texture_cube_map_array
#if !defined(GL_TEXTURE_CUBE_MAP_ARRAY)
#define GL_TEXTURE_CUBE_MAP_ARRAY 0x9009
#endif

#if defined(ANDROID)
#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#define XR_USE_PLATFORM_ANDROID 1
#elif defined(WIN32)
#include <unknwn.h>
#define XR_USE_GRAPHICS_API_OPENGL 1
#define XR_USE_PLATFORM_WIN32 1
#endif // defined(ANDROID)

#include <openxr/openxr.h>
// #include <meta_openxr_preview/openxr_oculus_helpers.h>
#include <openxr/openxr_platform.h>

// #include "Model/SceneView.h"
#include "Render/Framebuffer.h"
#include "Render/SurfaceRender.h"

std::string OXR_ResultToString(XrInstance instance, XrResult result);
void OXR_CheckErrors(XrInstance instance, XrResult result, const char *function,
                     bool failOnError);


struct xrJava {};

namespace OVRFW {

class XrApp;

typedef struct XrStructureHeader {
  XrStructureType type;
  void *XR_MAY_ALIAS next;
} XrStructureHeader;

class MainLoopContext {
public:
  MainLoopContext(xrJava &javaContext, XrApp *xrApp)
      : javaContext_(javaContext), xrApp_(xrApp) {}
  virtual ~MainLoopContext() { xrApp_ = nullptr; }

  const xrJava &GetJavaContext() const { return javaContext_; }
  virtual void HandleOsEvents() = 0;
  virtual bool ShouldExitMainLoop() const = 0;
  virtual bool IsExitRequested() const = 0;

protected:
  const xrJava javaContext_;
  class XrApp *xrApp_ = nullptr;
};

#if defined(ANDROID)
class ActivityMainLoopContext : public MainLoopContext {
public:
  ActivityMainLoopContext(xrJava &javaContext, XrApp *xrApp,
                          struct android_app *app)
      : MainLoopContext(javaContext, xrApp), app_(app) {}
  ~ActivityMainLoopContext() override { app_ = nullptr; }

  void HandleOsEvents() override;
  bool ShouldExitMainLoop() const override;
  bool IsExitRequested() const override;

private:
  struct android_app *app_ = nullptr;
};
#elif defined(WIN32)
class WindowsMainLoopContext : public MainLoopContext {
public:
  WindowsMainLoopContext(xrJava &javaContext, XrApp *xrApp)
      : MainLoopContext(javaContext, xrApp) {}
  ~WindowsMainLoopContext() override {}

  void HandleOsEvents() override;
  bool ShouldExitMainLoop() const override;
  bool IsExitRequested() const override;
};
#endif

class XrApp {
public:
  //     //============================
  //     // public interface
  //     enum ovrLifecycle { LIFECYCLE_UNKNOWN, LIFECYCLE_RESUMED,
  //     LIFECYCLE_PAUSED };
  //
  //     enum ovrRenderState {
  //         RENDER_STATE_LOADING, // show the loading icon
  //         RENDER_STATE_RUNNING, // render frames
  //     };

  static const int CPU_LEVEL = 2;
  static const int GPU_LEVEL = 3;
  static const int NUM_MULTI_SAMPLES = 4;
  static const int MAX_NUM_EYES = 2;
  static const int MAX_NUM_LAYERS = 16;

  //     XrApp(
  //         const int32_t mainThreadTid,
  //         const int32_t renderThreadTid,
  //         const int cpuLevel,
  //         const int gpuLevel)
  //         : BackgroundColor(0.0f, 0.6f, 0.1f, 1.0f),
  //           CpuLevel(cpuLevel),
  //           GpuLevel(gpuLevel),
  //           MainThreadTid(mainThreadTid),
  //           RenderThreadTid(renderThreadTid),
  //           NumFramebuffers(MAX_NUM_EYES) {}
  //     XrApp() : XrApp(0, 0, CPU_LEVEL, GPU_LEVEL) {}
  //     virtual ~XrApp() = default;

  // Android Service-based apps should just call MainLoop from their main thead
  void MainLoop(MainLoopContext &context);

// Android Activity-based apps and Windows apps can call Run from their entry
// point
#if defined(ANDROID)
  void Run(struct android_app *app);
#else
  void Run();
#endif // defined(ANDROID)

  //     //============================
  //     // public context interface
  //
  //     // Returns the application's context
  //     const xrJava* GetContext() const {
  //         return &Context;
  //     }
  //
  //     // App state share
  //     OVRFW::ovrFileSys* GetFileSys() {
  //         return FileSys.get();
  //     }
  OVRFW::ovrSurfaceRender &GetSurfaceRender() { return SurfaceRender; }
  //     OVRFW::OvrSceneView& GetScene() {
  //         return Scene;
  //     }
  //
  //     void SetRunWhilePaused(bool b) {
  //         RunWhilePaused = b;
  //     }
  //
  // #if defined(ANDROID)
  //     void HandleAndroidCmd(struct android_app* app, int32_t cmd);
  // #endif // defined(ANDROID)

  bool GetShouldExit() const { return ShouldExit; }
  void SetShouldExit(const bool b) { ShouldExit = b; }

  // #if defined(ANDROID)
  //     bool GetResumed() const {
  //         return Resumed;
  //     }
  //     bool GetSessionActive() const {
  //         return SessionActive;
  //     }
  // #endif // ANDROID
  //
protected:
  //     int GetNumFramebuffers() const {
  //         return NumFramebuffers;
  //     }
  //     ovrFramebuffer* GetFrameBuffer(int eye) {
  //         return &FrameBuffer[eye];
  //     }

  std::vector<XrExtensionProperties> GetXrExtensionProperties() const;
  //============================
  // App functions
  // All App* function can be overridden by the derived application class to
  // implement application-specific behaviors

  // Returns a list of OpenXr extensions needed for this app
  virtual std::vector<const char *> GetExtensions();

  // Apps can override this to return next chain that will be linked into
  // XrCreateInstanceInfo.next. Note that all of the structures returned in the
  // next chain must persist until the call to FreeInstanceCreateInfoNextChain()
  // at which point the app may free the structures if it allocated them
  // dynamically.
  virtual const void *GetInstanceCreateInfoNextChain();
  virtual void FreeInstanceCreateInfoNextChain(const void *nextChain);

  // Apps can override this to return next chain that will be linked into
  // XrCreateSessionInfo.next. Note that all of the structures returned in the
  // next chain must persist until the call to FreeSessionCreateInfoNextChain()
  // at which point the app may free the structures if it allocated them
  // dynamically.
  virtual const void *GetSessionCreateInfoNextChain();
  virtual void FreeSessionCreateInfoNextChain(const void *nextChain);

  //     virtual void GetInitialSceneUri(std::string& sceneUri) const;

  // Called when the application initializes.
  // Must return true if the application initializes successfully.
  virtual bool AppInit(const xrJava *context);
  // Called when the application shuts down
  virtual void AppShutdown(const xrJava *context);
  //     // Called when the application is resumed by the system.
  //     virtual void AppResumed(const xrJava* contet);
  //     // Called when the application is paused by the system.
  //     virtual void AppPaused(const xrJava* context);
  //     // Called when app loses focus
  //     virtual void AppLostFocus();
  //     // Called when app re-gains focus
  //     virtual void AppGainedFocus();
  // Called once per frame to allow the application to render eye buffers.
  virtual void AppRenderFrame(const OVRFW::ovrApplFrameIn &in,
                              OVRFW::ovrRendererOutput &out);
  // Called once per eye each frame for default renderer
  virtual void AppRenderEye(const OVRFW::ovrApplFrameIn &in,
                            OVRFW::ovrRendererOutput &out, int eye);
  // Called once per eye each frame for default renderer
  virtual void AppEyeGLStateSetup(const ovrApplFrameIn &in,
                                  const ovrFramebuffer *fb, int eye);

  virtual bool SessionInit();
  virtual void SessionEnd();
  virtual void SessionStateChanged(XrSessionState state);
  virtual void Update(const ovrApplFrameIn &in);
  virtual void Render(const ovrApplFrameIn &in, ovrRendererOutput &out);

  /// composition override
  union xrCompositorLayerUnion {
    XrCompositionLayerProjection Projection;
    XrCompositionLayerQuad Quad;
    XrCompositionLayerCylinderKHR Cylinder;
    XrCompositionLayerCubeKHR Cube;
    XrCompositionLayerEquirectKHR Equirect;
    XrCompositionLayerPassthroughFB Passthrough;
  };
  virtual void PreProjectionAddLayer(xrCompositorLayerUnion *, int &
                                     /*layerCount*/) {
    /// do nothing
  }
  /// Default proejction layer
  virtual void ProjectionAddLayer(xrCompositorLayerUnion *,
                                  int & /*layerCount*/);
  virtual void PostProjectionAddLayer(xrCompositorLayerUnion *,
                                      int & /*layerCount*/) {
    /// do nothing
  }

  // Add application specified SessionCreateInfo
  virtual void PreCreateSession(XrSessionCreateInfo &) {
    // do nothing
  }
  virtual void PostCreateSession(XrSessionCreateInfo &) {
    // do nothing
  }

  virtual void PreLocateViews(XrViewLocateInfo &) {
    // do nothing
  }

  // Returns a map from interaction profile paths to vectors of suggested
  // bindings. xrSuggestInteractionProfileBindings() is called once for each
  // interaction profile path in the returned map. Apps are encouraged to
  // suggest bindings for every device/interaction profile they support.
  // Override this for custom action bindings, or modify the default bindings.
  virtual std::unordered_map<XrPath, std::vector<XrActionSuggestedBinding>>
  GetSuggestedBindings(XrInstance instance);
  virtual void SuggestInteractionProfileBindings(
      const std::unordered_map<XrPath, std::vector<XrActionSuggestedBinding>>
          allSuggestedBindings);

  virtual void PreWaitFrame(XrFrameWaitInfo &) {}

  /// Xr Helpers
  XrInstance &GetInstance() { return Instance; };
  XrSession &GetSession() { return Session; }
  XrSystemId &GetSystemId() { return SystemId; }

  XrSpace &GetHeadSpace() { return HeadSpace; }
  XrSpace &GetLocalSpace() { return LocalSpace; }
  XrSpace &GetStageSpace() { return StageSpace; }
  XrSpace &GetCurrentSpace() { return CurrentSpace; }

  virtual XrActionSet CreateActionSet(uint32_t priority, const char *name,
                                      const char *localizedName);
  virtual XrAction CreateAction(XrActionSet actionSet, XrActionType type,
                                const char *actionName,
                                const char *localizedName,
                                int countSubactionPaths = 0,
                                XrPath *subactionPaths = nullptr);
  XrActionSuggestedBinding ActionSuggestedBinding(XrAction action,
                                                  const char *bindingString);
  XrSpace CreateActionSpace(XrAction poseAction, XrPath subactionPath);
  XrActionStateBoolean
  GetActionStateBoolean(XrAction action, XrPath subactionPath = XR_NULL_PATH);
  XrActionStateFloat GetActionStateFloat(XrAction action,
                                         XrPath subactionPath = XR_NULL_PATH);
  XrActionStateVector2f
  GetActionStateVector2(XrAction action, XrPath subactionPath = XR_NULL_PATH);
  bool ActionPoseIsActive(XrAction action, XrPath subactionPath);
  struct LocVel {
    XrSpaceLocation loc;
    XrSpaceVelocity vel;
  };
  XrApp::LocVel GetSpaceLocVel(XrSpace space, XrTime time);

  /// XR Input state overrides
  virtual void AttachActionSets();
  virtual void SyncActionSets(ovrApplFrameIn &in);

  // Called to deal with lifetime
  void HandleSessionStateChanges(XrSessionState state);

  virtual void AppHandleEvent(XrEventDataBaseHeader * /*baseEventHeader*/) {
    // do nothing
  }

protected:
  virtual XrInstance CreateInstance(const xrJava &context);
  virtual void DestroyInstance();
  virtual XrResult PollXrEvent(XrEventDataBuffer *eventDataBuffer);
  //     XrTime GetPrevDisplayTime() const {
  //         return PrevDisplayTime;
  //     }

private:
  // Called one time when the application process starts.
  // Returns true if the application initialized successfully.
  bool Init(const xrJava &context);

  // Called on each session creation
  bool InitSession();

  // Called on each session end
  void EndSession();

  // Called one time when the applicatoin process exits
  void Shutdown(const xrJava &context);

  // Called on each Shutdown, reset all member variable to initial state
  void Clear();

  //     // Called to handle any lifecycle state changes. This will call
  //     // AppPaused() and AppResumed()
  //     void HandleLifecycle(const xrJava* context);

  // Events
  virtual void HandleXrEvents();

  // Internal Input
  void HandleInput(ovrApplFrameIn &in);

  // Internal Render
  //     void RenderFrame(const ovrApplFrameIn& in, ovrRendererOutput& out);

public:
  OVR::Vector4f BackgroundColor;
  bool FreeMove{false};

protected:
  xrJava Context;
  //     ovrLifecycle Lifecycle = LIFECYCLE_UNKNOWN;
  ovrEgl Egl = {
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
      0,
      0,
      EGL_NO_DISPLAY,
      EGL_CAST(EGLConfig, 0),
      EGL_NO_SURFACE,
      EGL_NO_SURFACE,
      EGL_NO_CONTEXT
#endif // defined(XR_USE_GRAPHICS_API_OPENGL_ES)
  };

  // #if defined(ANDROID)
  //     bool Resumed = false;
  // #endif // defined(ANDROID)
  bool ShouldExit = false;
  bool Focused = false;

  // When set the framework will not bind any actions and will
  // skip calling SyncActionSets(), this is useful if an app
  // wants control over xrSyncAction
  // Note: This means input in ovrApplFrameIn won't be set
  bool SkipInputHandling = false;

  // An app can set this in AppInit() to change the resolution of the swapchain
  // allocated by the framework.
  float FramebufferResolutionScaleFactor{1.0f};

  XrVersion OpenXRVersion = XR_API_VERSION_1_0;
  XrInstance Instance = XR_NULL_HANDLE;
  XrSession Session = XR_NULL_HANDLE;
  XrViewConfigurationProperties ViewportConfig{
      XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
  XrViewConfigurationView ViewConfigurationView[MAX_NUM_EYES];
  XrCompositionLayerProjectionView ProjectionLayerElements[MAX_NUM_EYES];
  XrView Projections[MAX_NUM_EYES];
  XrPosef ViewTransform[MAX_NUM_EYES];
  XrSystemId SystemId = XR_NULL_SYSTEM_ID;
  XrSpace HeadSpace = XR_NULL_HANDLE;
  XrSpace LocalSpace = XR_NULL_HANDLE;
  XrSpace StageSpace = XR_NULL_HANDLE;
  XrSpace CurrentSpace = XR_NULL_HANDLE;
  bool SessionActive = false;

  XrActionSet BaseActionSet = XR_NULL_HANDLE;
  XrPath LeftHandPath = XR_NULL_PATH;
  XrPath RightHandPath = XR_NULL_PATH;
  XrAction AimPoseAction = XR_NULL_HANDLE;
  XrAction GripPoseAction = XR_NULL_HANDLE;
  XrAction JoystickAction = XR_NULL_HANDLE;
  XrAction thumbstickClickAction = XR_NULL_HANDLE;
  XrAction IndexTriggerAction = XR_NULL_HANDLE;
  XrAction IndexTriggerClickAction = XR_NULL_HANDLE;
  XrAction GripTriggerAction = XR_NULL_HANDLE;
  XrAction ButtonAAction = XR_NULL_HANDLE;
  XrAction ButtonBAction = XR_NULL_HANDLE;
  XrAction ButtonXAction = XR_NULL_HANDLE;
  XrAction ButtonYAction = XR_NULL_HANDLE;
  XrAction ButtonMenuAction = XR_NULL_HANDLE;
  /// common touch actions
  XrAction ThumbStickTouchAction = XR_NULL_HANDLE;
  XrAction ThumbRestTouchAction = XR_NULL_HANDLE;
  XrAction TriggerTouchAction = XR_NULL_HANDLE;

  XrSpace LeftControllerAimSpace = XR_NULL_HANDLE;
  XrSpace RightControllerAimSpace = XR_NULL_HANDLE;
  XrSpace LeftControllerGripSpace = XR_NULL_HANDLE;
  XrSpace RightControllerGripSpace = XR_NULL_HANDLE;
  uint32_t LastFrameAllButtons = 0u;
  uint32_t LastFrameAllTouches = 0u;

  OVRFW::ovrSurfaceRender SurfaceRender;
  // OVRFW::OvrSceneView Scene;
  //     std::unique_ptr<OVRFW::ovrFileSys> FileSys;
  //     std::unique_ptr<OVRFW::ModelFile> SceneModel;

private:
  XrTime PrevDisplayTime = 0.0;
  //     int SwapInterval;
  int CpuLevel = CPU_LEVEL;
  int GpuLevel = GPU_LEVEL;
  int MainThreadTid;
  //     int RenderThreadTid;

  xrCompositorLayerUnion Layers[MAX_NUM_LAYERS];
  int LayerCount;

  ovrFramebuffer FrameBuffer[MAX_NUM_EYES];
  //     int NumFramebuffers = MAX_NUM_EYES;
  //     bool IsAppFocused = false;
  //     bool RunWhilePaused = false;
  bool ShouldRender = true;
};

} // namespace OVRFW

#if defined(ANDROID)

#define ENTRY_POINT(appClass)                                                  \
  void android_main(struct android_app *app) {                                 \
    auto appl = std::make_unique<appClass>();                                  \
    appl->Run(app);                                                            \
  }

#elif defined(WIN32)

#define ENTRY_POINT(appClass)                                                  \
  __pragma(comment(linker, "/SUBSYSTEM:WINDOWS"));                             \
  int APIENTRY WinMain(HINSTANCE, HINSTANCE, PSTR, int) {                      \
    auto appl = std::make_unique<appClass>();                                  \
    appl->Run();                                                               \
    return 0;                                                                  \
  }

#else

#define ENTRY_POINT(appClass)                                                  \
  int main(int, const char **) {                                               \
    auto appl = std::make_unique<appClass>();                                  \
    appl->Run();                                                               \
    return 0;                                                                  \
  }

#endif // defined(ANDROID)

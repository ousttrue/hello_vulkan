#include "pipeline.hpp"
#include "swapchain_manager.hpp"
#include <assert.h>
#include <vko.h>

static double getCurrentTime() {
  timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    LOGE("clock_gettime() failed.\n");
    return 0.0;
  }
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

struct Dispatcher {
  bool _active = false;

  vko::Instance _instance;
  vko::PhysicalDevice _picked;
  std::shared_ptr<vko::Surface> _surface;
  vko::Device _device;
  VkQueue _grahicsQueue;

  std::shared_ptr<class Pipeline> _pipeline;
  std::shared_ptr<class SwapchainManager> _swapchain;

  unsigned _frameCount = 0;
  double _startTime = 0;

public:
  bool isReady() const { return _pipeline && _active; }
  void onResume();
  void onPause();
  void onInitWindow(ANativeWindow *window, AAssetManager *assetManager);
  void onTermWindow();
  bool onFrame(AAssetManager *assetManager);
};

void Dispatcher::onResume() { this->_active = true; }

void Dispatcher::onPause() { this->_active = false; }

void Dispatcher::onInitWindow(ANativeWindow *window,
                              AAssetManager *assetManager) {
  _instance._appInfo.pApplicationName = "Mali SDK";
  _instance._appInfo.pEngineName = "Mali SDK";
  _instance._instanceExtensions = {"VK_KHR_surface", "VK_KHR_android_surface"};
#ifdef NDEBUG
#else
  _instance._validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  _instance._instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  _instance._instanceExtensions.push_back("VK_EXT_debug_report");
#endif
  VK_CHECK(_instance.create());

  VkAndroidSurfaceCreateInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
      .flags = 0,
      .window = window,
  };
  VkSurfaceKHR surface;
  if (vkCreateAndroidSurfaceKHR(_instance, &info, nullptr, &surface) !=
      VK_SUCCESS) {
    LOGE("failed: vkCreateAndroidSurfaceKHR");
    return;
  }

  _picked = _instance.pickPhysicakDevice(surface);
  assert(_picked._physicalDevice);

  _surface = std::make_shared<vko::Surface>(_instance, surface,
                                            _picked._physicalDevice);

  _device._validationLayers = _instance._validationLayers;
  VK_CHECK(_device.create(_picked._physicalDevice, _picked._graphicsFamily,
                          _picked._presentFamily));

  _pipeline = Pipeline::create(_picked._physicalDevice, _device,
                               _surface->chooseSwapSurfaceFormat().format,
                               assetManager);

  _startTime = getCurrentTime();
}

void Dispatcher::onTermWindow() {
  vkDeviceWaitIdle(this->_device);

  this->_swapchain = {};
  this->_pipeline = {};
  // this->_device = {};
}

// exit mainloop if return false
bool Dispatcher::onFrame(AAssetManager *assetManager) {
  if (!this->_active) {
    return true;
  }

  if (!this->_pipeline) {
    return true;
  }

  if (!_swapchain) {
    _swapchain = SwapchainManager::create(
        _picked._physicalDevice, _surface->_surface, _device,
        _picked._graphicsFamily, _picked._presentFamily,
        _pipeline->renderPass(), nullptr);
    if (!_swapchain) {
      // error ?
      LOGE("no swapchain");
      return false;
    }
  }

  auto [res, acquireSemaphore, backbuffer] = _swapchain->AcquireNext();
  if (res == VK_SUCCESS) {
    // through next
  } else if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
    LOGE("[RESULT_ERROR_OUTDATED_SWAPCHAIN]");
    _swapchain->sync(acquireSemaphore);
    _swapchain = {};
    return true;
  } else {
    // error ?
    LOGE("Unrecoverable swapchain error.\n");
    _swapchain->sync(acquireSemaphore);
    return false;
  }

  // Signal the underlying context that we're using this backbuffer now.
  // This will also wait for all fences associated with this swapchain image
  // to complete first.
  // When submitting command buffer that writes to swapchain, we need to wait
  // for this semaphore first.
  // Also, delete the older semaphore.
  auto [oldSemaphore, cmd] = backbuffer->beginFrame(acquireSemaphore);
  _swapchain->addClearedSemaphore(oldSemaphore);
  _pipeline->render(cmd, backbuffer->framebuffer(), _swapchain->size());
  res = backbuffer->endFrame(_device._graphicsQueue, cmd, _device._presentQueue,
                             _swapchain->handle());

  _frameCount++;
  if (_frameCount == 100) {
    double endTime = getCurrentTime();
    LOGI("FPS: %.3f\n", _frameCount / (endTime - _startTime));
    _frameCount = 0;
    _startTime = endTime;
  }

  return true;
}
#include <magic_enum/magic_enum.hpp>

struct UserData {
  android_app *pApp = nullptr;
  Dispatcher *dispatcher = nullptr;
};

enum class cmd_names {
  APP_CMD_INPUT_CHANGED,
  APP_CMD_INIT_WINDOW,
  APP_CMD_TERM_WINDOW,
  APP_CMD_WINDOW_RESIZED,
  APP_CMD_WINDOW_REDRAW_NEEDED,
  APP_CMD_CONTENT_RECT_CHANGED,
  APP_CMD_GAINED_FOCUS,
  APP_CMD_LOST_FOCUS,
  APP_CMD_CONFIG_CHANGED,
  APP_CMD_LOW_MEMORY,
  APP_CMD_START,
  APP_CMD_RESUME,
  APP_CMD_SAVE_STATE,
  APP_CMD_PAUSE,
  APP_CMD_STOP,
  APP_CMD_DESTROY,
};

static void engineHandleCmd(android_app *pApp, int32_t _cmd) {
  auto *dispatcher = static_cast<UserData *>(pApp->userData)->dispatcher;
  auto cmd = static_cast<cmd_names>(_cmd);
  switch (cmd) {
  case cmd_names::APP_CMD_RESUME:
    LOGI("# %s", magic_enum::enum_name(cmd).data());
    dispatcher->onResume();
    break;

  case cmd_names::APP_CMD_PAUSE:
    LOGI("# %s", magic_enum::enum_name(cmd).data());
    dispatcher->onPause();
    break;

  case cmd_names::APP_CMD_INIT_WINDOW:
    LOGI("# %s", magic_enum::enum_name(cmd).data());
    dispatcher->onInitWindow(pApp->window, pApp->activity->assetManager);
    break;

  case cmd_names::APP_CMD_TERM_WINDOW:
    LOGI("# %s", magic_enum::enum_name(cmd).data());
    dispatcher->onTermWindow();
    break;

  default:
    LOGI("# %s: not handled", magic_enum::enum_name(cmd).data());
    break;
  }
}

void android_main(android_app *state) {
#ifdef NDEBUG
  LOGI("#### [release][android_main] ####");
#else
  LOGI("#### [debug][android_main] ####");
#endif

  Dispatcher dispatcher;
  UserData engine{
      .pApp = state,
      .dispatcher = &dispatcher,
  };
  state->userData = &engine;
  state->onAppCmd = engineHandleCmd;
  state->onInputEvent = [](android_app *, AInputEvent *) { return 0; };

  for (;;) {
    struct android_poll_source *source;
    int events;
    while ((ALooper_pollOnce(dispatcher.isReady() ? 0 : -1, nullptr, &events,
                             (void **)&source)) >= 0) {
      if (source) {
        source->process(state, source);
      }

      if (state->destroyRequested) {
        return;
      }
    }

    if (!dispatcher.onFrame(state->activity->assetManager)) {
      break;
    }
  }
}

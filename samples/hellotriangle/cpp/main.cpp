#include "userdata.h"

#include "pipeline.hpp"
#include "swapchain_manager.hpp"
#include <assert.h>
#include <vko.h>

class SemaphoreManager {
  VkDevice _device = VK_NULL_HANDLE;
  std::vector<VkSemaphore> _recycledSemaphores;
public:
  SemaphoreManager(VkDevice device) : _device(device) {}
  ~SemaphoreManager() {
    for (auto &semaphore : _recycledSemaphores) {
      vkDestroySemaphore(_device, semaphore, nullptr);
    }
  }
  VkSemaphore getClearedSemaphore() {
    if (_recycledSemaphores.empty()) {
      VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
      VkSemaphore semaphore;
      if (vkCreateSemaphore(_device, &info, nullptr, &semaphore) !=
          VK_SUCCESS) {
        LOGE("vkCreateSemaphore");
        abort();
      }
      return semaphore;
    } else {
      auto semaphore = _recycledSemaphores.back();
      _recycledSemaphores.pop_back();
      return semaphore;
    }
  }
  void addClearedSemaphore(VkSemaphore semaphore) {
    _recycledSemaphores.push_back(semaphore);
  }
};

auto APP_NAME = "hellotriangle";

static double getCurrentTime() {
  timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    LOGE("clock_gettime() failed.\n");
    return 0.0;
  }
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static bool main_loop(android_app *state, UserData *userdata) {
  LOGI("## main_loop");
  vko::Instance _instance;
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
      .window = state->window,
  };
  VkSurfaceKHR surface;
  VK_CHECK(vkCreateAndroidSurfaceKHR(_instance, &info, nullptr, &surface));

  vko::PhysicalDevice _picked = _instance.pickPhysicakDevice(surface);
  assert(_picked._physicalDevice);

  std::shared_ptr<vko::Surface> _surface = std::make_shared<vko::Surface>(
      _instance, surface, _picked._physicalDevice);

  vko::Device _device;
  _device._validationLayers = _instance._validationLayers;
  VK_CHECK(_device.create(_picked._physicalDevice, _picked._graphicsFamily,
                          _picked._presentFamily));

  std::shared_ptr<class Pipeline> _pipeline =
      Pipeline::create(_picked._physicalDevice, _device,
                       _surface->chooseSwapSurfaceFormat().format,
                       state->activity->assetManager);

  std::shared_ptr<class SwapchainManager> _swapchain;
  unsigned _frameCount = 0;
  auto _startTime = getCurrentTime();

  auto semaphoreManager = std::make_shared<SemaphoreManager>(_device);

  for (;;) {
    while (true) {
      if (!userdata->_active) {
        vkDeviceWaitIdle(_device);
        return false;
      }

      struct android_poll_source *source;
      int events;
      if (ALooper_pollOnce(
              // timeout 0 for vulkan animation
              0, nullptr, &events, (void **)&source) < 0) {
        break;
      }
      if (source) {
        source->process(state, source);
      }
      if (state->destroyRequested) {
        return true;
      }
    }

    if (!_swapchain) {
      _swapchain = SwapchainManager::create(
          _picked._physicalDevice, _surface->_surface, _device,
          _picked._graphicsFamily, _picked._presentFamily,
          _pipeline->renderPass(), nullptr);
      assert(_swapchain);
    }

    // void SwapchainManager::sync(VkSemaphore acquireSemaphore) {
    //   vkQueueWaitIdle(_presentationQueue);
    //   _semaphoreManager->addClearedSemaphore(acquireSemaphore);
    // }

    // void SwapchainManager::addClearedSemaphore(VkSemaphore semaphore) {
    //   // Recycle the old semaphore back into the semaphore manager.
    //   if (semaphore != VK_NULL_HANDLE) {
    //     _semaphoreManager->addClearedSemaphore(semaphore);
    //   }
    // }

    auto acquireSemaphore = semaphoreManager->getClearedSemaphore();
    auto [res, backbuffer] = _swapchain->AcquireNext(acquireSemaphore);
    if (res == VK_SUCCESS) {
      // through next
    } else if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
      LOGE("[RESULT_ERROR_OUTDATED_SWAPCHAIN]");
      vkQueueWaitIdle(_device._presentQueue);
      semaphoreManager->addClearedSemaphore(acquireSemaphore);
      _swapchain = {};
      // return true;
    } else {
      // error ?
      LOGE("Unrecoverable swapchain error.\n");
      vkQueueWaitIdle(_device._presentQueue);
      semaphoreManager->addClearedSemaphore(acquireSemaphore);
      return true;
    }

    // Signal the underlying context that we're using this backbuffer now.
    // This will also wait for all fences associated with this swapchain image
    // to complete first.
    // When submitting command buffer that writes to swapchain, we need to wait
    // for this semaphore first.
    // Also, delete the older semaphore.
    auto [oldSemaphore, cmd] = backbuffer->beginFrame(acquireSemaphore);
    if (oldSemaphore != VK_NULL_HANDLE) {
      semaphoreManager->addClearedSemaphore(oldSemaphore);
    }
    _pipeline->render(cmd, backbuffer->framebuffer(), _swapchain->size());
    res = backbuffer->endFrame(_device._graphicsQueue, cmd,
                               _device._presentQueue, _swapchain->handle());

    _frameCount++;
    if (_frameCount == 100) {
      double endTime = getCurrentTime();
      LOGI("FPS: %.3f\n", _frameCount / (endTime - _startTime));
      _frameCount = 0;
      _startTime = endTime;
    }
  }
}

void android_main(android_app *state) {

#ifdef NDEBUG
  __android_log_print(ANDROID_LOG_INFO, APP_NAME,
                      "#### [release][android_main] ####");
#else
  __android_log_print(ANDROID_LOG_INFO, APP_NAME,
                      "#### [debug][android_main] ####");
#endif

  UserData userdata{
      .pApp = state,
      ._appName = APP_NAME,
  };
  state->userData = &userdata;
  state->onAppCmd = UserData::on_app_cmd;
  state->onInputEvent = [](android_app *, AInputEvent *) { return 0; };

  for (;;) {
    auto is_exit = wait_window(state, &userdata);
    if (is_exit) {
      break;
    }

    is_exit = main_loop(state, &userdata);
    if (is_exit) {
      break;
    }
  }
}

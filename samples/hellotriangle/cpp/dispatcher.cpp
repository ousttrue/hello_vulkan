#include "dispatcher.h"
#include "device_manager.hpp"
#include "pipeline.hpp"
#include "swapchain_manager.hpp"
#include <assert.h>
#include <vko.h>
#include <vulkan/vulkan_core.h>

static double getCurrentTime() {
  timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    LOGE("clock_gettime() failed.\n");
    return 0.0;
  }
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

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

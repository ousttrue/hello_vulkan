#include "dispatcher.h"
#include "logger.hpp"
#include "device_manager.hpp"
#include "pipeline.hpp"
#include "swapchain_manager.hpp"
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
  std::vector<const char *> layers;
#ifdef NDEBUG
#else
  layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  _device = DeviceManager::create("Mali SDK", "Mali SDK", layers);
  if (!_device) {
    return;
  }
  if (!_device->createSurfaceFromAndroid(window)) {
    return;
  }
  auto gpu = _device->selectGpu();
  if (!gpu) {
    return;
  }
  if (!_device->createLogicalDevice(layers)) {
    return;
  }

  _pipeline =
      Pipeline::create(_device->Device, _device->getSurfaceFormat().format,
                       assetManager, _device->Selected.MemoryProperties);

  _startTime = getCurrentTime();
}

void Dispatcher::onTermWindow() {
  vkDeviceWaitIdle(this->_device->Device);

  this->_swapchain = {};
  this->_pipeline = {};
  this->_device = {};
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
        _device->Selected.Gpu, _device->Surface, _device->Device,
        _device->Selected.SelectedQueueFamilyIndex, _device->Queue,
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
    _swapchain->sync(_device->Queue, acquireSemaphore);
    _swapchain = {};
    return true;
  } else {
    // error ?
    LOGE("Unrecoverable swapchain error.\n");
    _swapchain->sync(_device->Queue, acquireSemaphore);
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
  res = backbuffer->endFrame(_device->Queue, cmd, _device->Queue,
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

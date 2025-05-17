#include "dispatcher.h"
#include "common.hpp"
#include "device_manager.hpp"
#include "pipeline.hpp"
#include "semaphore_manager.hpp"
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

  _semaphoreManager = std::make_shared<SemaphoreManager>(_device->Device);

  _pipeline =
      Pipeline::create(_device->Device, _device->getSurfaceFormat().format,
                       assetManager, _device->Selected.MemoryProperties);

  _startTime = getCurrentTime();
}

void Dispatcher::onTermWindow() {
  vkDeviceWaitIdle(this->_device->Device);

  this->_swapchain = {};
  this->_pipeline = {};
  this->_semaphoreManager = {};
  this->_device = {};
}

bool Dispatcher::onFrame(AAssetManager *assetManager) {
  if (this->_active && this->_device && this->_pipeline) {
    auto backbuffer = getBackbuffer(this->_pipeline->renderPass());

    // render
    auto cmd = this->_swapchain->beginRender(backbuffer);

    // We will only submit this once before it's recycled.
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &beginInfo);

    auto size = this->_swapchain->size();

    this->_pipeline->render(cmd, backbuffer->framebuffer(), size);

    // Submit it to the queue.
    this->_swapchain->submitSwapchain(cmd);

    // present
    this->_swapchain->presentImage(backbuffer->index());

    _frameCount++;
    if (_frameCount == 100) {
      double endTime = getCurrentTime();
      LOGI("FPS: %.3f\n", _frameCount / (endTime - _startTime));
      _frameCount = 0;
      _startTime = endTime;
    }
  }

  return true;
}

MaliSDK::Result Dispatcher::initSwapchain(VkRenderPass renderPass) {
  _swapchain = SwapchainManager::create(
      _device->Selected.Gpu, _device->Surface, _device->Device,
      _device->Selected.SelectedQueueFamilyIndex, _device->Queue,
      _device->Queue, renderPass, _swapchain ? _swapchain->Handle() : nullptr);
  if (!_swapchain) {
    return MaliSDK::RESULT_ERROR_GENERIC;
  }
  return MaliSDK::RESULT_SUCCESS;
}

MaliSDK::Result Dispatcher::acquireNextImage(unsigned *image,
                                             VkRenderPass renderPass) {
  if (!_swapchain) {
    // Recreate swapchain.
    if (initSwapchain(renderPass) == MaliSDK::RESULT_SUCCESS)
      return MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN;
    else
      return MaliSDK::RESULT_ERROR_GENERIC;
  }

  auto device = _device->Device;
  auto queue = _device->Queue;
  auto acquireSemaphore = _semaphoreManager->getClearedSemaphore();
  VkResult res = vkAcquireNextImageKHR(device, _swapchain->Handle(), UINT64_MAX,
                                       acquireSemaphore, VK_NULL_HANDLE, image);

  if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
    vkQueueWaitIdle(queue);
    _semaphoreManager->addClearedSemaphore(acquireSemaphore);

    // Recreate swapchain.
    if (initSwapchain(renderPass) == MaliSDK::RESULT_SUCCESS)
      return MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN;
    else
      return MaliSDK::RESULT_ERROR_GENERIC;
  } else if (res != VK_SUCCESS) {
    vkQueueWaitIdle(queue);
    _semaphoreManager->addClearedSemaphore(acquireSemaphore);
    return MaliSDK::RESULT_ERROR_GENERIC;
  } else {
    // Signal the underlying context that we're using this backbuffer now.
    // This will also wait for all fences associated with this swapchain image
    // to complete first.
    // When submitting command buffer that writes to swapchain, we need to wait
    // for this semaphore first.
    // Also, delete the older semaphore.
    auto oldSemaphore = _swapchain->beginFrame(*image, acquireSemaphore);

    // Recycle the old semaphore back into the semaphore manager.
    if (oldSemaphore != VK_NULL_HANDLE) {
      _semaphoreManager->addClearedSemaphore(oldSemaphore);
    }

    return MaliSDK::RESULT_SUCCESS;
  }
}

std::shared_ptr<Backbuffer> Dispatcher::getBackbuffer(VkRenderPass renderPass) {
  unsigned index;
  for (auto res = this->acquireNextImage(&index, renderPass); true;
       res = this->acquireNextImage(&index, renderPass)) {
    if (res == MaliSDK::RESULT_SUCCESS) {
      break;
    }
    if (res == MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN) {
      // Handle Outdated error in acquire.
      LOGE("[RESULT_ERROR_OUTDATED_SWAPCHAIN]");
      continue;
    }
    // error
    LOGE("Unrecoverable swapchain error.\n");
    return {};
  }

  // swapchain current backbuffer
  return _swapchain->getBackbuffer(index);
}

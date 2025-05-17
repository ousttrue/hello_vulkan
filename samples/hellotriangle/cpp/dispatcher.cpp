#include "dispatcher.h"
#include "common.hpp"
#include "device_manager.hpp"
#include "pipeline.hpp"
#include "platform.hpp"
#include <vulkan/vulkan_core.h>

/// @brief Get the current monotonic time in seconds.
/// @returns Current time.
static double getCurrentTime() {
  timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    LOGE("clock_gettime() failed.\n");
    return 0.0;
  }
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void Dispatcher::onResume() { this->active = true; }

void Dispatcher::onPause() { this->active = false; }

void Dispatcher::onInitWindow(ANativeWindow *window,
                              AAssetManager *assetManager) {
  pPlatform = Platform::create(window);
  // pVulkanApp = VulkanApplication::create(
  //     pPlatform.get(), pPlatform->swapchainDimensions.format, assetManager);
  pPipeline = Pipeline::create(pPlatform->getDevice(),
                               pPlatform->_device->getSurfaceFormat().format,
                               assetManager);
  pPipeline->initVertexBuffer(pPlatform->getMemoryProperties());

  this->startTime = getCurrentTime();
}

void Dispatcher::onTermWindow() {
  vkDeviceWaitIdle(this->pPlatform->getDevice());

  this->pPipeline = {};
  this->pPlatform = {};
}

bool Dispatcher::onFrame(AAssetManager *assetManager) {
  if (this->active && this->pPlatform && this->pPipeline) {
    auto backbuffer = pPlatform->getBackbuffer(this->pPipeline->renderPass());

    // render
    auto cmd = this->pPlatform->_swapchain->beginRender(backbuffer);

    // We will only submit this once before it's recycled.
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &beginInfo);

    auto dim = this->pPlatform->_swapchain->getSwapchainDimesions();

    pPipeline->render(cmd, backbuffer->framebuffer, dim.width, dim.height);

    // Submit it to the queue.
    pPlatform->_swapchain->submitSwapchain(cmd);

    // present
    this->pPlatform->_swapchain->presentImage(backbuffer->index);

    frameCount++;
    if (frameCount == 100) {
      double endTime = getCurrentTime();
      LOGI("FPS: %.3f\n", frameCount / (endTime - startTime));
      frameCount = 0;
      startTime = endTime;
    }
  }

  return true;
}

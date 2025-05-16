#include "dispatcher.h"
#include "common.hpp"
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
                               pPlatform->surfaceFormat(), assetManager);
  pPipeline->initVertexBuffer(pPlatform->getMemoryProperties());
  pPlatform->updateSwapchain(pPipeline->renderPass());

  this->startTime = getCurrentTime();
}

void Dispatcher::onTermWindow() {
  this->pPipeline = {};
  this->pPlatform = {};
}

bool Dispatcher::onFrame(AAssetManager *assetManager) {
  if (this->active && this->pPlatform && this->pPipeline) {
    unsigned index;
    for (auto res = this->pPlatform->acquireNextImage(&index); true;
         res = this->pPlatform->acquireNextImage(&index)) {
      if (res == MaliSDK::RESULT_SUCCESS) {
        break;
      }
      if (res == MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN) {
        // Handle Outdated error in acquire.
        LOGE("[RESULT_ERROR_OUTDATED_SWAPCHAIN]");
        this->pPlatform->updateSwapchain(this->pPipeline->renderPass());
        continue;
      }
      // error
      LOGE("Unrecoverable swapchain error.\n");
      return false;
    }

    // swapchain current backbuffer
    auto backbuffer = pPlatform->getBackbuffer(index);

    // render
    auto cmd = this->pPlatform->beginRender(backbuffer);
    auto dim = this->pPlatform->getSwapchainDimesions();

    pPipeline->render(cmd, backbuffer->framebuffer, dim.width, dim.height);

    // Submit it to the queue.
    pPlatform->submitSwapchain(cmd);

    // present
    this->pPlatform->presentImage(backbuffer->index);

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

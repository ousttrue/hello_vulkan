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
  pPlatform = MaliSDK::Platform::create(window);
  // pVulkanApp = VulkanApplication::create(
  //     pPlatform.get(), pPlatform->swapchainDimensions.format, assetManager);
  pPipeline =
      Pipeline::create(pPlatform->getDevice(),
                       pPlatform->swapchainDimensions.format, assetManager);
  pPipeline->initVertexBuffer(pPlatform->getMemoryProperties());
  pPlatform->updateSwapchain(pPlatform->swapchainImages,
                             pPlatform->swapchainDimensions,
                             pPipeline->renderPass());

  this->startTime = getCurrentTime();
}

void Dispatcher::onTermWindow() {
  this->pPipeline = {};
  this->pPlatform = {};
}

bool Dispatcher::onFrame(AAssetManager *assetManager) {
  if (this->active && this->pPlatform && this->pPipeline) {
    // swapchain current backbuffer
    auto backbuffer = nextFrame();
    if (!backbuffer) {
      // swapchain error. exit
      return false;
    }

    // render
    auto cmd = this->pPlatform->beginRender(
        backbuffer, this->pPlatform->swapchainDimensions.width,
        this->pPlatform->swapchainDimensions.height);

    pPipeline->render(cmd, backbuffer->framebuffer,
                      this->pPlatform->swapchainDimensions.width,
                      this->pPlatform->swapchainDimensions.height);

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

std::shared_ptr<class Backbuffer> Dispatcher::nextFrame() {
  unsigned index;
  auto res = this->pPlatform->acquireNextImage(&index);
  while (res == MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN) {
    res = this->pPlatform->acquireNextImage(&index);
    this->pPlatform->updateSwapchain(this->pPlatform->swapchainImages,
                                      this->pPlatform->swapchainDimensions,
                                      this->pPipeline->renderPass());
    // // Handle Outdated error in acquire.
    // if (res != MaliSDK::RESULT_SUCCESS &&
    //     res != MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN) {
    //   return false;
    // }
  }
  if (res != MaliSDK::RESULT_SUCCESS) {
    LOGE("Unrecoverable swapchain error.\n");
    return {};
  }
  return pPlatform->backbuffers[index];
}

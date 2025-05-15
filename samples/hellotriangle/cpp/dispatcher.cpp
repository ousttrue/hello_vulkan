#include "dispatcher.h"
#include "application.hpp"
#include "common.hpp"
#include "platform.hpp"

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
  pVulkanApp =
      MaliSDK::VulkanApplication::create(pPlatform.get(), assetManager);
  this->startTime = getCurrentTime();
}

void Dispatcher::onTermWindow() {
  this->pVulkanApp = {};
  this->pPlatform = {};
}

bool Dispatcher::onFrame(AAssetManager *assetManager) {
  if (!this->active) {
    return true;
  }
  if (this->pPlatform && this->pVulkanApp) {
    // swapchain current backbuffer
    unsigned index;
    auto res = this->pPlatform->acquireNextImage(&index);
    while (res == MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN) {
      res = this->pPlatform->acquireNextImage(&index);
      this->pVulkanApp->updateSwapchain(assetManager,
                                        this->pPlatform->swapchainImages,
                                        this->pPlatform->swapchainDimensions);
    }
    if (res != MaliSDK::RESULT_SUCCESS) {
      LOGE("Unrecoverable swapchain error.\n");
      return false;
    }

    // render
    this->pVulkanApp->render(index, 0.0166f);

    // present
    res = this->pPlatform->presentImage(index);
    // Handle Outdated error in acquire.
    if (res != MaliSDK::RESULT_SUCCESS &&
        res != MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN) {
      return false;
    }

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

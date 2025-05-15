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

void Dispatcher::onPause() {
  this->active = false;
  this->pPlatform->onPause();
}

void Dispatcher::onInitWindow(ANativeWindow *window,
                              AAssetManager *assetManager) {
  if (this->pPlatform->initialize() != MaliSDK::RESULT_SUCCESS) {
    LOGE("Failed to initialize platform.\n");
    abort();
  }

  this->pPlatform->setNativeWindow(window);

  auto dim = this->pPlatform->getPreferredSwapchain();

  LOGI("Creating window!\n");
  if (this->pPlatform->createWindow(dim) != MaliSDK::RESULT_SUCCESS) {
    LOGE("Failed to create Vulkan window.\n");
    abort();
  }

  LOGI("Initializing application!\n");
  if (!this->pVulkanApp->initialize(this->pPlatform)) {
    LOGE("Failed to initialize Vulkan application.\n");
    abort();
  }

  // if (this->pVulkanApp) {
  // dim = this->pPlatform->getPreferredSwapchain();
  this->pPlatform->onResume(dim);

  LOGI("Updating swapchain!\n");
  this->pVulkanApp->updateSwapchain(this->pPlatform->swapchainImages,
                                    this->pPlatform->swapchainDimensions);

  this->startTime = getCurrentTime();
}

void Dispatcher::onTermWindow() {
  if (this->pVulkanApp) {
    this->pVulkanApp->terminate();
    delete this->pVulkanApp;
    this->pVulkanApp = nullptr;
    this->pPlatform->terminate();
  }
}

bool Dispatcher::onFrame() {
  if (!this->active) {
    return true;
  }
  if (!this->pPlatform->pNativeWindow) {
    return true;
  }

  unsigned index;
  auto res = this->pPlatform->acquireNextImage(&index);
  while (res == MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN) {
    res = this->pPlatform->acquireNextImage(&index);
    this->pVulkanApp->updateSwapchain(this->pPlatform->swapchainImages,
                                      this->pPlatform->swapchainDimensions);
  }

  if (res != MaliSDK::RESULT_SUCCESS) {
    LOGE("Unrecoverable swapchain error.\n");
    return false;
  }

  this->pVulkanApp->render(index, 0.0166f);
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

  return true;
}

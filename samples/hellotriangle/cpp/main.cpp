#include "application.hpp"
#include "common.hpp"
#include "android.hpp"

#include <android_native_app_glue.h>

struct AndroidState {
  struct android_app *pApp;
  MaliSDK::VulkanApplication *pVulkanApp;
  bool active;
};

static void engineHandleCmd(android_app *pApp, int32_t cmd) {
  auto *state = static_cast<AndroidState *>(pApp->userData);
  auto &platform =
      static_cast<MaliSDK::AndroidPlatform &>(MaliSDK::Platform::get());

  switch (cmd) {
  case APP_CMD_RESUME: {
    state->active = true;
    MaliSDK::Platform::SwapchainDimensions dim =
        platform.getPreferredSwapchain();

    if (state->pVulkanApp) {
      LOGI("Resuming swapchain!\n");
      platform.onResume(dim);

      std::vector<VkImage> images;
      platform.getCurrentSwapchain(&images, &dim);
      state->pVulkanApp->updateSwapchain(images, dim);
    }
    break;
  }

  case APP_CMD_PAUSE: {
    LOGI("Pausing swapchain!\n");
    state->active = false;
    platform.onPause();
    break;
  }

  case APP_CMD_INIT_WINDOW:
    if (pApp->window != nullptr) {
      LOGI("Initializing platform!\n");
      if (platform.initialize() != MaliSDK::RESULT_SUCCESS) {
        LOGE("Failed to initialize platform.\n");
        abort();
      }

      auto &platform =
          static_cast<MaliSDK::AndroidPlatform &>(MaliSDK::Platform::get());
      platform.setNativeWindow(state->pApp->window);

      MaliSDK::Platform::SwapchainDimensions dim =
          platform.getPreferredSwapchain();

      LOGI("Creating window!\n");
      if (platform.createWindow(dim) != MaliSDK::RESULT_SUCCESS) {
        LOGE("Failed to create Vulkan window.\n");
        abort();
      }

      LOGI("Creating application!\n");
      state->pVulkanApp = MaliSDK::createApplication();

      LOGI("Initializing application!\n");
      if (!state->pVulkanApp->initialize(&platform.getContext())) {
        LOGE("Failed to initialize Vulkan application.\n");
        abort();
      }

      LOGI("Updating swapchain!\n");
      std::vector<VkImage> images;
      platform.getCurrentSwapchain(&images, &dim);
      state->pVulkanApp->updateSwapchain(images, dim);
    }
    break;

  case APP_CMD_TERM_WINDOW:
    if (state->pVulkanApp) {
      LOGI("Terminating application!\n");
      state->pVulkanApp->terminate();
      delete state->pVulkanApp;
      state->pVulkanApp = nullptr;
      platform.terminate();
    }
    break;
  }
}

void android_main(android_app *state) {
  LOGI("Entering android_main()!\n");

  AndroidState engine;
  memset(&engine, 0, sizeof(engine));
  engine.pApp = state;

  state->userData = &engine;
  state->onAppCmd = engineHandleCmd;
  state->onInputEvent = [](android_app *, AInputEvent *) { return 0; };

  engine.pApp = state;

  auto &platform =
      static_cast<MaliSDK::AndroidPlatform &>(MaliSDK::Platform::get());
  static_cast<MaliSDK::AndroidAssetManager &>(MaliSDK::OS::getAssetManager())
      .setAssetManager(state->activity->assetManager);

  unsigned frameCount = 0;
  double startTime = MaliSDK::OS::getCurrentTime();

  for (;;) {
    struct android_poll_source *source;
    int ident;
    int events;

    while (
        (ident = ALooper_pollOnce(engine.pVulkanApp && engine.active ? 0 : -1,
                                  nullptr, &events, (void **)&source)) >= 0) {
      if (source)
        source->process(state, source);

      if (state->destroyRequested)
        return;
    }

    if (engine.pVulkanApp && engine.active) {
      unsigned index;
      std::vector<VkImage> images;
      MaliSDK::Platform::SwapchainDimensions dim;

      auto res = platform.acquireNextImage(&index);
      while (res == MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN) {
        platform.acquireNextImage(&index);
        platform.getCurrentSwapchain(&images, &dim);
        engine.pVulkanApp->updateSwapchain(images, dim);
      }

      if (res != MaliSDK::RESULT_SUCCESS) {
        LOGE("Unrecoverable swapchain error.\n");
        break;
      }

      engine.pVulkanApp->render(index, 0.0166f);
      res = platform.presentImage(index);

      // Handle Outdated error in acquire.
      if (res != MaliSDK::RESULT_SUCCESS &&
          res != MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN)
        break;

      frameCount++;
      if (frameCount == 100) {
        double endTime = MaliSDK::OS::getCurrentTime();
        LOGI("FPS: %.3f\n", frameCount / (endTime - startTime));
        frameCount = 0;
        startTime = endTime;
      }
    }
  }
}

#pragma once
#include <android_native_app_glue.h>
#include <vulkan/vulkan_core.h>

namespace MaliSDK {
class WSIPlatform;
class VulkanApplication;
} // namespace MaliSDK

struct Dispatcher {
  MaliSDK::WSIPlatform *pPlatform = nullptr;
  MaliSDK::VulkanApplication *pVulkanApp = nullptr;
  bool active = false;

  unsigned frameCount = 0;
  double startTime = 0;

  bool isReady() const { return this->pVulkanApp && this->active; }
  void onResume();
  void onPause();
  void onInitWindow(ANativeWindow *window, AAssetManager *assetManager);
  void onTermWindow();
  bool onFrame();
};

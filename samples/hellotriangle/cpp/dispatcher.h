#pragma once
#include <android_native_app_glue.h>
#include <vulkan/vulkan_core.h>

#include <memory>

struct Dispatcher {
  std::shared_ptr<class Platform> pPlatform;
  std::shared_ptr<class Pipeline> pPipeline;
  bool active = false;

  unsigned frameCount = 0;
  double startTime = 0;

public:
  bool isReady() const { return this->pPipeline && this->active; }
  void onResume();
  void onPause();
  void onInitWindow(ANativeWindow *window, AAssetManager *assetManager);
  void onTermWindow();
  bool onFrame(AAssetManager *assetManager);
};

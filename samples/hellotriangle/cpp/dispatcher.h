#pragma once
#include <android_native_app_glue.h>
#include <vulkan/vulkan_core.h>

#include <memory>

struct Dispatcher {
  bool _active = false;

  std::shared_ptr<class DeviceManager> _device;
  std::shared_ptr<class Pipeline> _pipeline;
  std::shared_ptr<class SwapchainManager> _swapchain;

  unsigned _frameCount = 0;
  double _startTime = 0;

public:
  bool isReady() const { return _pipeline && _active; }
  void onResume();
  void onPause();
  void onInitWindow(ANativeWindow *window, AAssetManager *assetManager);
  void onTermWindow();
  bool onFrame(AAssetManager *assetManager);
};

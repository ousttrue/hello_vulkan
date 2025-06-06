#pragma once
#include <android_native_app_glue.h>
#include <memory>
#include <vko.h>

struct Dispatcher {
  bool _active = false;

  vko::Instance _instance;
  vko::PhysicalDevice _picked;
  std::shared_ptr<vko::Surface> _surface;
  vko::Device _device;
  VkQueue _grahicsQueue;

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

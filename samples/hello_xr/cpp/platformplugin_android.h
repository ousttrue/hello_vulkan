#pragma once
#include "platformplugin.h"

struct PlatformData {
#ifdef XR_USE_PLATFORM_ANDROID
  void *applicationVM;
  void *applicationActivity;
#endif
};

std::shared_ptr<IPlatformPlugin>
CreatePlatformPlugin_Android(const std::shared_ptr<struct Options> &options,
                             const std::shared_ptr<PlatformData> &data);

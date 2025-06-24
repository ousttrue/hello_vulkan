#pragma once
#include "../vuloxr.h"
#ifdef XR_USE_PLATFORM_ANDROID
#include <android_native_app_glue.h>
#endif
#ifdef XR_USE_PLATFORM_WIN32
#include <Unknwn.h>
#endif
#include <openxr/openxr_platform.h>

namespace vuloxr {

namespace xr {

[[noreturn]] inline void ThrowXrResult(XrResult res,
                                       const char *originator = nullptr,
                                       const char *sourceLocation = nullptr) {
  Throw(fmt("XrResult failure [%d]", res), originator, sourceLocation);
}

inline XrResult CheckXrResult(XrResult res, const char *originator = nullptr,
                              const char *sourceLocation = nullptr) {
  if (XR_FAILED(res)) {
    ThrowXrResult(res, originator, sourceLocation);
  }

  return res;
}

struct Instance : NonCopyable {
  XrInstance instance = XR_NULL_HANDLE;
  std::vector<const char *> extensions;
  XrInstanceCreateInfo createInfo{
      .type = XR_TYPE_INSTANCE_CREATE_INFO,
      // .next = next,
      .createFlags = 0,
      .applicationInfo =
          {.applicationName = "HelloXR",
           // Current version is 1.1.x, but hello_xr only requires 1.0.x
           .applicationVersion = {},
           .engineName = {},
           .engineVersion = {},
           .apiVersion = XR_API_VERSION_1_0},
      .enabledApiLayerCount = 0,
      .enabledApiLayerNames = nullptr,
      .enabledExtensionCount = 0,
  };

  XrSystemId systemId = XR_NULL_SYSTEM_ID;
  XrSystemGetInfo systemInfo{
      .type = XR_TYPE_SYSTEM_GET_INFO,
      .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
  };

  ~Instance() {
    if (this->instance != XR_NULL_HANDLE) {
      Logger::Info("xro::Instance::~Instance ...");
      xrDestroyInstance(this->instance);
    }
  }

  XrResult create(void *next) {
    for (auto name : this->extensions) {
      Logger::Info("openxr extension: %s", name);
    }
    if (this->extensions.size()) {
      this->createInfo.enabledExtensionCount =
          static_cast<uint32_t>(this->extensions.size());
      this->createInfo.enabledExtensionNames = extensions.data();
    }
    this->createInfo.next = next;
    auto result = xrCreateInstance(&this->createInfo, &this->instance);
    if (result != XR_SUCCESS) {
      return result;
    }

    result = xrGetSystem(this->instance, &this->systemInfo, &this->systemId);
    if (result != XR_SUCCESS) {
      return result;
    }

    // Read graphics properties for preferred swapchain length and logging.
    XrSystemProperties systemProperties{
        .type = XR_TYPE_SYSTEM_PROPERTIES,
    };
    CheckXrResult(xrGetSystemProperties(this->instance, this->systemId,
                                        &systemProperties));

    // Log system properties.
    Logger::Info("System Properties: Name=%s VendorId=%d",
                 systemProperties.systemName, systemProperties.vendorId);
    Logger::Info(
        "System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
        systemProperties.graphicsProperties.maxSwapchainImageWidth,
        systemProperties.graphicsProperties.maxSwapchainImageHeight,
        systemProperties.graphicsProperties.maxLayerCount);
    Logger::Info("System Tracking Properties: OrientationTracking=%s "
                 "PositionTracking=%s",
                 systemProperties.trackingProperties.orientationTracking ==
                         XR_TRUE
                     ? "True"
                     : "False",
                 systemProperties.trackingProperties.positionTracking == XR_TRUE
                     ? "True"
                     : "False");

    return XR_SUCCESS;
  }

};

} // namespace xr

} // namespace vuloxr

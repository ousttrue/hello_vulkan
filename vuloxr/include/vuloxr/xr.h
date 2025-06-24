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

struct Platform : NonCopyable {
  std::vector<XrExtensionProperties> extensions;

  Platform() {
    uint32_t ext_count = 0;
    CheckXrResult(
        xrEnumerateInstanceExtensionProperties(NULL, 0, &ext_count, NULL));
    this->extensions.resize(ext_count, {XR_TYPE_EXTENSION_PROPERTIES, NULL});
    CheckXrResult(xrEnumerateInstanceExtensionProperties(
        NULL, ext_count, &ext_count, this->extensions.data()));
    for (uint32_t i = 0; i < ext_count; i++) {
      Logger::Verbose("InstanceExt[%2d/%2d]: %s\n", i, ext_count,
                      this->extensions[i].extensionName);
    }
  }
};

struct Instance : NonCopyable {
  XrInstance instance = XR_NULL_HANDLE;
  std::vector<const char *> extensions;
  //     extensions.push_back ("XR_KHR_android_create_instance");
  //     extensions.push_back ("XR_KHR_opengl_es_enable");
  // #if defined (USE_OXR_HANDTRACK)
  //     extensions.push_back (XR_EXT_HAND_TRACKING_EXTENSION_NAME);
  //     extensions.push_back (XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME);
  //     extensions.push_back (XR_FB_HAND_TRACKING_AIM_EXTENSION_NAME);
  //     extensions.push_back (XR_FB_HAND_TRACKING_CAPSULES_EXTENSION_NAME);
  // #endif
  // #if defined (USE_OXR_PASSTHROUGH)
  //     extensions.push_back (XR_FB_PASSTHROUGH_EXTENSION_NAME);
  // #endif

  XrInstanceCreateInfo createInfo{
      .type = XR_TYPE_INSTANCE_CREATE_INFO,
      // .next = next,
      .createFlags = 0,
      .applicationInfo =
          {
              .applicationName = "HelloXR",
              // Current version is 1.1.x, but hello_xr only requires 1.0.x
              .applicationVersion = {},
              .engineName = {},
              .engineVersion = {},
              // .apiVersion = XR_API_VERSION_1_0,
              .apiVersion = XR_CURRENT_API_VERSION,
          },
      .enabledApiLayerCount = 0,
      .enabledApiLayerNames = nullptr,
      .enabledExtensionCount = 0,
  };

  XrSystemId systemId = XR_NULL_SYSTEM_ID;
  XrSystemGetInfo systemInfo{
      .type = XR_TYPE_SYSTEM_GET_INFO,
      .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
  };

  XrSystemProperties systemProperties{
      .type = XR_TYPE_SYSTEM_PROPERTIES,
  };
  // #if defined(USE_OXR_HANDTRACK)
  //   XrSystemHandTrackingPropertiesEXT handTrackProp{
  //       XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT};
  //   prop.next = &handTrackProp;
  // #endif

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

    //
    //     /* query instance name, version */
    //     XrInstanceProperties prop = {XR_TYPE_INSTANCE_PROPERTIES};
    //     xrGetInstanceProperties (instance, &prop);
    //     LOGI("OpenXR Instance Runtime   : \"%s\", Version: %u.%u.%u",
    //     prop.runtimeName,
    //             XR_VERSION_MAJOR (prop.runtimeVersion),
    //             XR_VERSION_MINOR (prop.runtimeVersion),
    //             XR_VERSION_PATCH (prop.runtimeVersion));
    //
    //     return instance;
    // }

    result = xrGetSystem(this->instance, &this->systemInfo, &this->systemId);
    if (result != XR_SUCCESS) {
      return result;
    }

    CheckXrResult(xrGetSystemProperties(this->instance, this->systemId,
                                        &this->systemProperties));

    return XR_SUCCESS;
  }

  // Log system properties.
  void logSystemProperties() {
    Logger::Info("System Properties: Name=%s VendorId=%d",
                 this->systemProperties.systemName,
                 this->systemProperties.vendorId);
    Logger::Info(
        "System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
        this->systemProperties.graphicsProperties.maxSwapchainImageWidth,
        this->systemProperties.graphicsProperties.maxSwapchainImageHeight,
        this->systemProperties.graphicsProperties.maxLayerCount);
    Logger::Info(
        "System Tracking Properties: OrientationTracking=%s "
        "PositionTracking=%s",
        this->systemProperties.trackingProperties.orientationTracking == XR_TRUE
            ? "True"
            : "False",
        this->systemProperties.trackingProperties.positionTracking == XR_TRUE
            ? "True"
            : "False");
    // #if defined(USE_OXR_HANDTRACK)
    //   LOGI("System HandTracking Props : %d",
    //   handTrackProp.supportsHandTracking);
    // #endif
  }
};

} // namespace xr
} // namespace vuloxr

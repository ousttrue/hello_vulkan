#pragma once
#include <openxr/openxr.h>
#include <vko/vko.h>

#define XRO_CHECK(cmd) xro::CheckXrResult(cmd, #cmd, FILE_AND_LINE);
// #define CHECK_XRRESULT(res, cmdStr) CheckXrResult(res, cmdStr,
// FILE_AND_LINE);

namespace xro {

[[noreturn]] inline void ThrowXrResult(XrResult res,
                                       const char *originator = nullptr,
                                       const char *sourceLocation = nullptr) {
  vko::Throw(vko::fmt("XrResult failure [%d]", res), originator,
             sourceLocation);
}

#define THROW_XR(xr, cmd) ThrowXrResult(xr, #cmd, FILE_AND_LINE);

inline XrResult CheckXrResult(XrResult res, const char *originator = nullptr,
                              const char *sourceLocation = nullptr) {
  if (XR_FAILED(res)) {
    ThrowXrResult(res, originator, sourceLocation);
  }

  return res;
}

struct Instance {

  using Logger = vko::Logger;

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

  ~Instance() {
    if (this->instance != XR_NULL_HANDLE) {
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
    return XR_SUCCESS;
  }
};

} // namespace xro

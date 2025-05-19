// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include <windows.h>
#include <combaseapi.h>
#include "platformplugin.h"
#include "to_string.h"
#include "check.h"

[[noreturn]] inline void ThrowHResult(HRESULT hr,
                                      const char *originator = nullptr,
                                      const char *sourceLocation = nullptr) {
  Throw(Fmt("HRESULT failure [%x]", hr), originator, sourceLocation);
}

inline HRESULT CheckHResult(HRESULT hr, const char *originator = nullptr,
                            const char *sourceLocation = nullptr) {
  if (FAILED(hr)) {
    ThrowHResult(hr, originator, sourceLocation);
  }

  return hr;
}

#define THROW_HR(hr, cmd) ThrowHResult(hr, #cmd, FILE_AND_LINE);
#define CHECK_HRCMD(cmd) CheckHResult(cmd, #cmd, FILE_AND_LINE);
#define CHECK_HRESULT(res, cmdStr) CheckHResult(res, cmdStr, FILE_AND_LINE);


namespace {
struct Win32PlatformPlugin : public IPlatformPlugin {
  Win32PlatformPlugin(const std::shared_ptr<Options> &) {
    CHECK_HRCMD(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
  }

  virtual ~Win32PlatformPlugin() override { CoUninitialize(); }

  std::vector<std::string> GetInstanceExtensions() const override { return {}; }

  XrBaseInStructure *GetInstanceCreateExtension() const override {
    return nullptr;
  }

  void
  UpdateOptions(const std::shared_ptr<struct Options> & /*unused*/) override {}
};
} // namespace

std::shared_ptr<IPlatformPlugin>
CreatePlatformPlugin_Win32(const std::shared_ptr<Options> &options) {
  return std::make_shared<Win32PlatformPlugin>(options);
}

// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <locale>
#include <openxr/openxr.h>
#include <vulkan/vulkan_core.h>

bool EqualsIgnoreCase(const std::string &s1, const std::string &s2,
                      const std::locale &loc = std::locale());

XrFormFactor GetXrFormFactor(const std::string &formFactorStr);

XrViewConfigurationType
GetXrViewConfigurationType(const std::string &viewConfigurationStr);

XrEnvironmentBlendMode
GetXrEnvironmentBlendMode(const std::string &environmentBlendModeStr);

const char *
GetXrEnvironmentBlendModeStr(XrEnvironmentBlendMode environmentBlendMode);

std::string GetXrVersionString(XrVersion ver);

struct Options {
  std::string FormFactor{"Hmd"};
  std::string ViewConfiguration{"Stereo"};
  std::string EnvironmentBlendMode{"Opaque"};
  std::string AppSpace{"Local"};

  Options() = default;
  ~Options() = default;
  Options(const Options &) = delete;
  Options &operator=(const Options &) = delete;

  struct {
    XrFormFactor FormFactor{XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};
    XrViewConfigurationType ViewConfigType{
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
    XrEnvironmentBlendMode EnvironmentBlendMode{
        XR_ENVIRONMENT_BLEND_MODE_OPAQUE};
  } Parsed;

  void ParseStrings();
  VkClearColorValue GetBackgroundClearColor() const;
  void SetEnvironmentBlendMode(XrEnvironmentBlendMode environmentBlendMode);

  bool UpdateOptionsFromCommandLine(int argc, char *argv[]);
  bool UpdateOptionsFromSystemProperties();
};

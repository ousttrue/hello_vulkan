// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "xr_linear.h"
#include <memory>
#include <openxr/openxr.h>
#include <set>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

class VulkanGraphicsPlugin;

class OpenXrProgram {
  const struct Options &m_options;
  XrInstance m_instance;
  XrSystemId m_systemId{XR_NULL_SYSTEM_ID};
  const std::set<XrEnvironmentBlendMode> m_acceptableBlendModes;
  VkDebugUtilsMessengerEXT m_vkDebugUtilsMessenger{VK_NULL_HANDLE};

  OpenXrProgram(const Options &options, XrInstance instance,
                XrSystemId systemId);

public:
  ~OpenXrProgram();

  // Create an Instance and other basic instance-level initialization.
  static std::shared_ptr<OpenXrProgram>
  Create(const Options &options,
         const std::vector<std::string> &platformExtensions, void *next);

  // Initialize the graphics device for the selected system.
  struct VulkanResources {
    VkInstance Instance;
    VkPhysicalDevice PhysicalDevice;
    VkDevice Device;
    uint32_t QueueFamilyIndex;
  };
  VulkanResources
  InitializeVulkan(const std::vector<const char *> &layers,
                   const std::vector<const char *> &instanceExtensions,
                   const std::vector<const char *> &deviceExtensions,
                   const VkDebugUtilsMessengerCreateInfoEXT *debugInfo);

  // Get preferred blend mode based on the view configuration specified in the
  // Options
  XrEnvironmentBlendMode GetPreferredBlendMode() const;

  // Create a Session and other basic session-level initialization.
  std::shared_ptr<class OpenXrSession>
  InitializeSession(VulkanResources vulkan);
};

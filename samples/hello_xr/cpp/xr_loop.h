#pragma once
#include <functional>
#include <openxr/openxr.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

void xr_loop(const std::function<bool(bool)> &runLoop, XrInstance instance,
             XrSystemId systemId, XrSession session, XrSpace appSpace,
             XrEnvironmentBlendMode blendMode, VkClearColorValue clearColor,
             XrViewConfigurationType viewConfigurationType, VkFormat viewFormat,
             VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex,
             VkDevice device);

#pragma once
#include <functional>
#include <openxr/openxr.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vuloxr/vk.h>

void xr_main_loop(
    const std::function<bool(bool)> &runLoop, XrInstance instance,
    XrSystemId systemId, XrSession session, XrSpace appSpace,
    //
    VkFormat viewFormat, const vuloxr::vk::PhysicalDevice &physicalDevice,
    uint32_t queueFamilyIndex, VkDevice device,
    //
    VkClearColorValue clearColor,
    XrEnvironmentBlendMode blendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
    XrViewConfigurationType viewConfigurationType =
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO);

#pragma once
#include <functional>
#include <openxr/openxr.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

void xr_vulkan_session(
    const std::function<bool(bool)> &runLoop, XrInstance instance,
    XrSystemId systemId, XrSession session, XrSpace appSpace,
    //
    VkFormat viewFormat, VkPhysicalDevice physicalDevice,
    uint32_t queueFamilyIndex, VkDevice device,
    //
    VkClearColorValue clearColor,
    XrEnvironmentBlendMode blendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
    XrViewConfigurationType viewConfigurationType =
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO);

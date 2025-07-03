#pragma once
#include <functional>
#include <openxr/openxr.h>

#ifdef XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vuloxr/vk.h>
struct XrGraphics {
  VkFormat format;
  VkPhysicalDevice physicalDevice;
  uint32_t queueFamilyIndex;
  VkDevice device;
  VkClearColorValue clearColor;
};
#endif

void xr_main_loop(
    const std::function<bool(bool)> &runLoop, XrInstance instance,
    XrSystemId systemId, XrSession session, XrSpace appSpace,
    //
    const XrGraphics &graphics,
    //
    XrEnvironmentBlendMode blendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
    XrViewConfigurationType viewConfigurationType =
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO);

#pragma once
#include <functional>
#include <openxr/openxr.h>
#include <span>

#ifdef XR_USE_GRAPHICS_API_VULKAN
#include <vuloxr/vk/swapchain.h>
using Graphics = vuloxr::vk::Vulkan;
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
#include <vuloxr/xr/graphics/egl.h>
using Graphics = vuloxr::egl::OpenGLES;
#endif

void xr_main_loop(
    const std::function<bool(bool)> &runLoop, XrInstance instance,
    XrSystemId systemId, XrSession session, XrSpace appSpace,
    std::span<const int64_t> formats,
    //
    const Graphics &graphics,
    //
    XrColor4f clearColor,
    XrEnvironmentBlendMode blendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
    XrViewConfigurationType viewConfigurationType =
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO);

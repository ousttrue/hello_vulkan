#pragma once
#include <functional>
#include <span>

#ifdef XR_USE_GRAPHICS_API_VULKAN
#include <vuloxr/vk/swapchain.h>
using Graphics = vuloxr::vk::Vulkan;
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
#include <vuloxr/xr/graphics/egl.h>
using Graphics = vuloxr::egl::OpenGLES;
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL
#include <vuloxr/xr/graphics/gl.h>
using Graphics = vuloxr::gl::OpenGL;
#endif

#include <openxr/openxr.h>
#include <vuloxr/xr/swapchain.h>

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
using SwapchainImageType = XrSwapchainImageOpenGLESKHR;
const SwapchainImageType SwapchainImage{
    .type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR,
};
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL
using SwapchainImageType = XrSwapchainImageOpenGLKHR;
const SwapchainImageType SwapchainImage{
    .type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR,
};
#endif

#ifdef XR_USE_GRAPHICS_API_VULKAN
using SwapchainImageType = XrSwapchainImageVulkan2KHR;
const SwapchainImageType SwapchainImage{
    .type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR,
};
#endif

using GraphicsSwapchain = vuloxr::xr::Swapchain<SwapchainImageType>;

void xr_main_loop(
    const std::function<bool(bool)> &runLoop, XrInstance instance,
    XrSystemId systemId, XrSession session, XrSpace appSpace,
    std::span<const int64_t> formats,
    //
    const Graphics &graphics,
    //
    const XrColor4f &clearColor,
    XrEnvironmentBlendMode blendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
    XrViewConfigurationType viewConfigurationType =
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO);

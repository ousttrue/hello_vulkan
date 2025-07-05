#pragma once
#include <DirectXMath.h>
#include <functional>
#include <span>

#ifdef XR_USE_GRAPHICS_API_VULKAN
#include <vuloxr/vk/swapchain.h>
using Graphics = vuloxr::vk::Vulkan;

#if defined(XR_USE_GRAPHICS_API_OPENGL)
static_assert(false, "VULKAN + OPENGL");
#endif
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
static_assert(false, "VULKAN + OPENGL_ES");
#endif

#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
#include <vuloxr/xr/graphics/egl.h>
using Graphics = vuloxr::egl::OpenGLES;

#if defined(XR_USE_GRAPHICS_API_OPENGL)
static_assert(false, "OPENGL_ES + OPENGL");
#endif
#if defined(XR_USE_GRAPHICS_API_VULKAN)
static_assert(false, "OPENGL_ES + VULKAN");
#endif

#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL
#include <vuloxr/xr/graphics/gl.h>
using Graphics = vuloxr::gl::OpenGL;

#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
static_assert(false, "OPENGL + OPENGL_ES");
#endif
#if defined(XR_USE_GRAPHICS_API_VULKAN)
static_assert(false, "OPENGL + VULKAN");
#endif

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

struct VertexAttributeLayout {
  uint32_t components;
  uint32_t offset;
};

struct InputData {
  const void *data = nullptr;
  uint32_t stride = 0;
  uint32_t drawCount = 0;

  uint32_t byteSize() const { return stride * drawCount; }
};

struct ViewRenderer {
  struct Impl *_impl;
  ViewRenderer(const Graphics *_graphics,
               const std::shared_ptr<GraphicsSwapchain> &swapchain);
  ~ViewRenderer();
  void initScene(const char *vs, const char *fs,
                 std::span<const VertexAttributeLayout> layouts,
                 const InputData &vertices, const InputData &indices = {});
  void initSwapchain(int width, int height,
                     std::span<const SwapchainImageType> images);
  void render(uint32_t index, const XrColor4f &clearColor,
              std::span<const DirectX::XMFLOAT4X4> matrices = {});
};

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

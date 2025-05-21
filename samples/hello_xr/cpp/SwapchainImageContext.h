#pragma once
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#ifdef XR_USE_PLATFORM_WIN32
#include <Unknwn.h>
#endif
#ifdef XR_USE_PLATFORM_ANDROID
#include <android_native_app_glue.h>
#endif

#include <openxr/openxr_platform.h>

#include "VertexBuffer.h"

class SwapchainImageContext {
  VkDevice m_vkDevice{VK_NULL_HANDLE};

  // A packed array of XrSwapchainImageVulkan2KHR's for
  // xrEnumerateSwapchainImages
  std::vector<XrSwapchainImageVulkan2KHR> m_swapchainImages;
  std::vector<std::shared_ptr<class RenderTarget>> m_renderTarget;
  VkExtent2D m_size{};
  std::shared_ptr<class RenderPass> m_rp;

  // XrStructureType swapchainImageType;
  //
  // SwapchainImageContext(XrStructureType _swapchainImageType)
  //     : swapchainImageType(_swapchainImageType) {}

public:
  std::shared_ptr<class Pipeline> m_pipe;
  std::shared_ptr<class DepthBuffer> depthBuffer;

  // static std::shared_ptr<SwapchainImageContext>
  // create(XrStructureType _swapchainImageType) {
  //   return std::shared_ptr<SwapchainImageContext>(
  //       new SwapchainImageContext(_swapchainImageType));
  // }

  std::vector<XrSwapchainImageBaseHeader *>
  Create(VkDevice device,
         const std::shared_ptr<class MemoryAllocator> &memAllocator,
         uint32_t capacity, VkExtent2D size, VkFormat format,
         VkSampleCountFlagBits sampleCount,
         const std::shared_ptr<class PipelineLayout> &layout,
         const std::shared_ptr<class ShaderProgram> &sp,
         const std::shared_ptr<VertexBuffer> &vb);

  uint32_t ImageIndex(const XrSwapchainImageBaseHeader *swapchainImageHeader) {
    auto p = reinterpret_cast<const XrSwapchainImageVulkan2KHR *>(
        swapchainImageHeader);
    return (uint32_t)(p - &m_swapchainImages[0]);
  }

  void BindRenderTarget(uint32_t index,
                        VkRenderPassBeginInfo *renderPassBeginInfo);
};

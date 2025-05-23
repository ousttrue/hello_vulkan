#pragma once
#include <map>
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
  std::map<VkImage, std::shared_ptr<class RenderTarget>> m_renderTarget;
  VkExtent2D m_size{};
  std::shared_ptr<class RenderPass> m_rp;

public:
  std::shared_ptr<class PipelineLayout> m_pipelineLayout{};
  std::shared_ptr<struct VertexBuffer> m_drawBuffer;
  std::shared_ptr<class Pipeline> m_pipe;
  std::shared_ptr<class DepthBuffer> m_depthBuffer;
  std::vector<XrSwapchainImageBaseHeader *> m_bases;

  static std::shared_ptr<SwapchainImageContext>
  Create(VkDevice device,
         const std::shared_ptr<class MemoryAllocator> &memAllocator,
         uint32_t capacity, VkExtent2D size, VkFormat format,
         VkSampleCountFlagBits sampleCount,
         const std::shared_ptr<class ShaderProgram> &sp);

  uint32_t ImageIndex(const XrSwapchainImageBaseHeader *swapchainImageHeader) {
    auto p = reinterpret_cast<const XrSwapchainImageVulkan2KHR *>(
        swapchainImageHeader);
    return (uint32_t)(p - &m_swapchainImages[0]);
  }

  void BindRenderTarget(VkImage image,
                        VkRenderPassBeginInfo *renderPassBeginInfo);

  void RenderView(VkCommandBuffer cmd, VkImage image,
                  const Vec4 &clearColor, const std::vector<Mat4> &cubes);
};

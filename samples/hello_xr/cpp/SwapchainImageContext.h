#pragma once
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include <Unknwn.h>
#include <Windows.h>

#include <openxr/openxr_platform.h>

#include "VertexBuffer.h"

// VkImage + framebuffer wrapper
class RenderTarget {
  VkImage colorImage{VK_NULL_HANDLE};
  VkImage depthImage{VK_NULL_HANDLE};
  VkImageView colorView{VK_NULL_HANDLE};
  VkImageView depthView{VK_NULL_HANDLE};
  VkDevice m_vkDevice{VK_NULL_HANDLE};

  RenderTarget() = default;

public:
  VkFramebuffer fb{VK_NULL_HANDLE};
  ~RenderTarget();

  RenderTarget(RenderTarget &&other) noexcept;
  RenderTarget &operator=(RenderTarget &&other) noexcept;
  static std::shared_ptr<RenderTarget>
  Create(VkDevice device, VkImage aColorImage, VkImage aDepthImage,
         VkExtent2D size, class RenderPass &renderPass);

  RenderTarget(const RenderTarget &) = delete;
  RenderTarget &operator=(const RenderTarget &) = delete;
};

class SwapchainImageContext {
  VkDevice m_vkDevice{VK_NULL_HANDLE};

  // A packed array of XrSwapchainImageVulkan2KHR's for
  // xrEnumerateSwapchainImages
  std::vector<XrSwapchainImageVulkan2KHR> swapchainImages;
  std::vector<std::shared_ptr<class RenderTarget>> renderTarget;
  VkExtent2D size{};
  std::shared_ptr<class RenderPass> rp;
  XrStructureType swapchainImageType;

  SwapchainImageContext(XrStructureType _swapchainImageType)
      : swapchainImageType(_swapchainImageType) {}

public:
  std::shared_ptr<class Pipeline> pipe;
  std::shared_ptr<class DepthBuffer> depthBuffer;

  static std::shared_ptr<SwapchainImageContext>
  create(XrStructureType _swapchainImageType) {
    return std::shared_ptr<SwapchainImageContext>(
        new SwapchainImageContext(_swapchainImageType));
  }

  std::vector<XrSwapchainImageBaseHeader *>
  Create(VkDevice device,
         const std::shared_ptr<class MemoryAllocator> &memAllocator,
         uint32_t capacity, const XrSwapchainCreateInfo &swapchainCreateInfo,
         const std::shared_ptr<class PipelineLayout> &layout,
         const std::shared_ptr<class ShaderProgram> &sp,
         const std::shared_ptr<VertexBuffer> &vb);

  uint32_t ImageIndex(const XrSwapchainImageBaseHeader *swapchainImageHeader) {
    auto p = reinterpret_cast<const XrSwapchainImageVulkan2KHR *>(
        swapchainImageHeader);
    return (uint32_t)(p - &swapchainImages[0]);
  }

  void BindRenderTarget(uint32_t index,
                        VkRenderPassBeginInfo *renderPassBeginInfo);
};

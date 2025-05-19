#pragma once
#include <array>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

std::string vkResultString(VkResult res);

[[noreturn]] void ThrowVkResult(VkResult res, const char *originator = nullptr,
                                const char *sourceLocation = nullptr);

VkResult CheckVkResult(VkResult res, const char *originator = nullptr,
                       const char *sourceLocation = nullptr);



// VkImage + framebuffer wrapper
struct RenderTarget {
  VkImage colorImage{VK_NULL_HANDLE};
  VkImage depthImage{VK_NULL_HANDLE};
  VkImageView colorView{VK_NULL_HANDLE};
  VkImageView depthView{VK_NULL_HANDLE};
  VkFramebuffer fb{VK_NULL_HANDLE};

  RenderTarget() = default;
  ~RenderTarget();

  RenderTarget(RenderTarget &&other) noexcept;
  RenderTarget &operator=(RenderTarget &&other) noexcept;
  void Create(const class VulkanDebugObjectNamer &namer, VkDevice device,
              VkImage aColorImage, VkImage aDepthImage, VkExtent2D size,
              class RenderPass &renderPass);

  RenderTarget(const RenderTarget &) = delete;
  RenderTarget &operator=(const RenderTarget &) = delete;

private:
  VkDevice m_vkDevice{VK_NULL_HANDLE};
};

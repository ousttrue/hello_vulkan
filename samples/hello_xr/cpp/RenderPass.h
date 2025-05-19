#pragma once
#include <vulkan/vulkan.h>

// RenderPass wrapper
struct RenderPass {
  VkFormat colorFmt{};
  VkFormat depthFmt{};
  VkRenderPass pass{VK_NULL_HANDLE};

  RenderPass() = default;
  bool Create(const class VulkanDebugObjectNamer &namer, VkDevice device,
              VkFormat aColorFmt, VkFormat aDepthFmt);
  ~RenderPass();
  RenderPass(const RenderPass &) = delete;
  RenderPass &operator=(const RenderPass &) = delete;
  RenderPass(RenderPass &&) = delete;
  RenderPass &operator=(RenderPass &&) = delete;

private:
  VkDevice m_vkDevice{VK_NULL_HANDLE};
};

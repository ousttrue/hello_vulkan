#pragma once
#include <memory>
#include <vulkan/vulkan.h>

// RenderPass wrapper
class RenderPass {
  VkDevice m_vkDevice{VK_NULL_HANDLE};

  RenderPass() {}

public:
  VkFormat colorFmt{};
  VkFormat depthFmt{};
  VkRenderPass pass{VK_NULL_HANDLE};

  static std::shared_ptr<RenderPass>
  Create(const class VulkanDebugObjectNamer &namer, VkDevice device,
         VkFormat aColorFmt, VkFormat aDepthFmt);
  ~RenderPass();
  RenderPass(const RenderPass &) = delete;
  RenderPass &operator=(const RenderPass &) = delete;
  RenderPass(RenderPass &&) = delete;
  RenderPass &operator=(RenderPass &&) = delete;
};

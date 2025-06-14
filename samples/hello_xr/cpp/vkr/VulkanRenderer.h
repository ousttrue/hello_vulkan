#pragma once
#include <common/FloatTypes.h>
#include <map>
#include <memory>
#include <vector>
#include <vko/vko_pipeline.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

class VulkanRenderer {
  VkPhysicalDevice m_physicalDevice;
  VkDevice m_device;
  uint32_t m_queueFamilyIndex;

  std::shared_ptr<class MemoryAllocator> m_memAllocator;

  std::shared_ptr<class DepthBuffer> m_depthBuffer;

  std::map<VkImage, std::shared_ptr<class RenderTarget>> m_renderTarget;

public:
  std::shared_ptr<struct VertexBuffer> m_drawBuffer;
  VulkanRenderer(VkPhysicalDevice physicalDevice, VkDevice device,
                 uint32_t queueFamilyIndex, VkExtent2D size,
                 VkFormat depthFormath, VkSampleCountFlagBits sampleCount);
  void RenderView(VkCommandBuffer cmd, VkRenderPass renderPass,
                  VkPipelineLayout pipelineLayout, VkPipeline pipeline,
                  VkImage image, VkExtent2D size, VkFormat colorFormat,
                  VkFormat depthFormat, const Vec4 &clearColor,
                  const std::vector<Mat4> &cubes);
};

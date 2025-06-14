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

  VkExtent2D m_size{};
  VkFormat m_colorFormat;
  VkFormat m_depthFormat;
  std::shared_ptr<class DepthBuffer> m_depthBuffer;

  std::shared_ptr<class Pipeline> m_pipeline;
  std::shared_ptr<struct VertexBuffer> m_drawBuffer;

  std::map<VkImage, std::shared_ptr<class RenderTarget>> m_renderTarget;

public:
  VulkanRenderer(VkPhysicalDevice physicalDevice, VkDevice device,
                 uint32_t queueFamilyIndex, VkExtent2D size, VkFormat format,
                 VkSampleCountFlagBits sampleCount);
  void RenderView(VkCommandBuffer cmd, VkImage image, const Vec4 &clearColor,
                  const std::vector<Mat4> &cubes);
};

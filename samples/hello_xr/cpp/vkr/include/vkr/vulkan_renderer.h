#pragma once
#include <common/FloatTypes.h>
#include <map>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include <vko/vko_pipeline.h>

class VulkanRenderer {
  VkPhysicalDevice m_physicalDevice;
  VkDevice m_device;
  uint32_t m_queueFamilyIndex;

  VkQueue m_queue{VK_NULL_HANDLE};
  VkSemaphore m_vkDrawDone{VK_NULL_HANDLE};
  std::shared_ptr<class MemoryAllocator> m_memAllocator;
  std::shared_ptr<class ShaderProgram> m_shaderProgram{};
  std::shared_ptr<class CmdBuffer> m_cmdBuffer;

  VkExtent2D m_size{};
  std::shared_ptr<class PipelineLayout> m_pipelineLayout{};
  std::shared_ptr<class DepthBuffer> m_depthBuffer;
  std::shared_ptr<class Pipeline> m_pipe;
  std::shared_ptr<class RenderPass> m_rp;
  std::shared_ptr<struct VertexBuffer> m_drawBuffer;

  std::map<VkImage, std::shared_ptr<class RenderTarget>> m_renderTarget;

public:
  VulkanRenderer(VkPhysicalDevice physicalDevice, VkDevice device,
                 uint32_t queueFamilyIndex, VkExtent2D size, VkFormat format,
                 VkSampleCountFlagBits sampleCount);

  // Render to a swapchain image for a projection view.
  VkCommandBuffer BeginCommand();
  void EndCommand(VkCommandBuffer cmd);

  void RenderView(VkCommandBuffer cmd, VkImage image, const Vec4 &clearColor,
                  const std::vector<Mat4> &cubes);
};

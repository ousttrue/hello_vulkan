#pragma once
#include "FloatTypes.h"
#include <map>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

class VulkanRenderer {
  VkDevice m_vkDevice{VK_NULL_HANDLE};
  VkExtent2D m_size{};
  std::shared_ptr<class PipelineLayout> m_pipelineLayout{};
  std::shared_ptr<class DepthBuffer> m_depthBuffer;
  std::shared_ptr<class Pipeline> m_pipe;
  std::shared_ptr<class RenderPass> m_rp;
  std::shared_ptr<struct VertexBuffer> m_drawBuffer;

  std::map<VkImage, std::shared_ptr<class RenderTarget>> m_renderTarget;

public:
  static std::shared_ptr<VulkanRenderer>
  Create(VkDevice device,
         const std::shared_ptr<class MemoryAllocator> &memAllocator,
         VkExtent2D size, VkFormat format, VkSampleCountFlagBits sampleCount,
         const std::shared_ptr<class ShaderProgram> &sp);

  void RenderView(VkCommandBuffer cmd, VkImage image, const Vec4 &clearColor,
                  const std::vector<Mat4> &cubes);
};

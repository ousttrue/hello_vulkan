#pragma once
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

// Pipeline wrapper for rendering pipeline state
class Pipeline {
  VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  std::vector<VkDynamicState> dynamicStateEnables;
  VkDevice m_vkDevice{VK_NULL_HANDLE};

  Pipeline() {}
public:
  VkRenderPass m_renderPass = VK_NULL_HANDLE;
  VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
  VkPipeline m_pipeline = VK_NULL_HANDLE;
  ~Pipeline();
  static std::shared_ptr<Pipeline>
  Create(VkDevice device, VkExtent2D size, VkFormat colorFormat,
         VkFormat depthFormat, const std::shared_ptr<struct VertexBuffer> &vb);
};

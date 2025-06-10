#pragma once
#include <memory>
#include <vulkan/vulkan.h>

class PipelineObject {
  VkDevice _device;

  std::shared_ptr<struct DescriptorSetLayout> _descriptorSetLayout;
  std::shared_ptr<struct PipelineLayout> _pipelineLayout;
  VkRenderPass _renderPass;

  std::shared_ptr<class BufferObject> _vertexBuffer;
  std::shared_ptr<BufferObject> _indexBuffer;
  std::shared_ptr<class TextureObject> _texture;
  VkImageView _textureImageView = VK_NULL_HANDLE;
  VkSampler _textureSampler = VK_NULL_HANDLE;
  VkClearValue _clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

  // after swapchain
  VkPipeline _graphicsPipeline = VK_NULL_HANDLE;

  PipelineObject(
      VkDevice device,
      const std::shared_ptr<DescriptorSetLayout> &descriptorSetLayout,
      const std::shared_ptr<PipelineLayout> &pipelineLayout,
      VkRenderPass renderPass);

public:
  ~PipelineObject();
  static std::shared_ptr<PipelineObject>
  create(VkPhysicalDevice physicalDevice, VkDevice device,
         uint32_t graphicsQueueFamilyIndex, VkFormat swapchainFormat);
  void createGraphicsPipeline(VkExtent2D swapchainExtent);
  void record(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer,
              VkExtent2D extent, VkDescriptorSet descriptorSet);

  VkRenderPass renderPass() const;
  VkDescriptorSetLayout descriptorSetLayout() const;
  std::tuple<VkImageView, VkSampler> texture() const {
    return {_textureImageView, _textureSampler};
  }
};

#pragma once
#include <memory>
#include <vko/vko_pipeline.h>
#include <vulkan/vulkan_core.h>

struct Vec2 {
  float x;
  float y;
};

struct Vec3 {
  float x;
  float y;
  float z;
};

struct Vertex {
  Vec2 pos;
  Vec3 color;
  Vec2 texCoord;
};

struct Matrix {
  float m[16];
};

struct UniformBufferObject {
  Matrix model;
  Matrix view;
  Matrix proj;
  void setTime(float time, float width, float height);
};

class PipelineObject {
  VkDevice _device;

  VkDescriptorSetLayout _descriptorSetLayout;
  VkPipelineLayout _pipelineLayout;
  VkRenderPass _renderPass;

  std::shared_ptr<vko::Buffer> _vertexBuffer;
  std::shared_ptr<vko::Buffer> _indexBuffer;
  std::shared_ptr<vko::Image> _texture;
  VkImageView _textureImageView = VK_NULL_HANDLE;
  VkSampler _textureSampler = VK_NULL_HANDLE;
  VkClearValue _clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

  // after swapchain
  VkPipeline _graphicsPipeline = VK_NULL_HANDLE;

public:
  PipelineObject(VkPhysicalDevice physicalDevice, VkDevice device,
                 uint32_t graphicsQueueFamilyIndex, VkFormat swapchainFormat,
                 VkDescriptorSetLayout descriptorSetLayout);
  ~PipelineObject();
  void createGraphicsPipeline(VkExtent2D swapchainExtent);
  void record(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer,
              VkExtent2D extent, VkDescriptorSet descriptorSet);

  VkRenderPass renderPass() const;
  std::tuple<VkImageView, VkSampler> texture() const {
    return {_textureImageView, _textureSampler};
  }
};

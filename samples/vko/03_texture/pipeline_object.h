#pragma once
#include <vko/vko_pipeline.h>
#include <vulkan/vulkan_core.h>

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

  VkClearValue _clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

  // after swapchain
  VkPipeline _graphicsPipeline = VK_NULL_HANDLE;

public:
  PipelineObject(VkPhysicalDevice physicalDevice, VkDevice device,
                 uint32_t graphicsQueueFamilyIndex, VkFormat swapchainFormat,
                 VkDescriptorSetLayout descriptorSetLayout);
  ~PipelineObject();
  void createGraphicsPipeline(const struct Scene &scene,
                              VkExtent2D swapchainExtent);
  void record(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer,
              VkExtent2D extent, VkDescriptorSet descriptorSet,
              const struct Scene &scene);

  VkRenderPass renderPass() const;
};

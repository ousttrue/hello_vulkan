#pragma once
#include <vko/vko_pipeline.h>
#include <vulkan/vulkan_core.h>

struct PipelineObject {
  VkDevice device;
  VkPipelineLayout pipelineLayout;
  VkRenderPass renderPass;
  VkPipeline graphicsPipeline = VK_NULL_HANDLE;

  PipelineObject(
      VkPhysicalDevice physicalDevice, VkDevice _device,
      //
      VkFormat swapchainFormat, VkExtent2D swapchainExtent,
      //
      VkDescriptorSetLayout descriptorSetLayout,
      const VkVertexInputBindingDescription &vertexInputBindingDescription,
      const std::vector<VkVertexInputAttributeDescription>
          &attributeDescriptions);
  ~PipelineObject();
};

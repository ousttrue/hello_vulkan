#pragma once
#include <vko/vko_pipeline.h>

vko::Pipeline createPipelineObject(
    VkPhysicalDevice physicalDevice, VkDevice _device,
    //
    VkFormat swapchainFormat, VkExtent2D swapchainExtent,
    //
    VkDescriptorSetLayout descriptorSetLayout,
    const VkVertexInputBindingDescription &vertexInputBindingDescription,
    const std::vector<VkVertexInputAttributeDescription>
        &attributeDescriptions);

#pragma once
#include <memory>
#include <stdexcept>
#include <vulkan/vulkan.h>

struct PipelineLayout {
  VkDevice _device;
  VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };

  PipelineLayout(VkDevice device, VkDescriptorSetLayout setLayout)
      : _device(device) {
    this->pipelineLayoutInfo.setLayoutCount = 1;
    this->pipelineLayoutInfo.pSetLayouts =
        &setLayout; // &descriptorSetLayout->_descriptorSetLayout,
    // VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr,
                               &_pipelineLayout) != VK_SUCCESS) {
      throw std::runtime_error("failed to create pipeline layout!");
    }
  }

  ~PipelineLayout() {
    vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
  }

  static std::shared_ptr<PipelineLayout>
  create(VkDevice device, VkDescriptorSetLayout setLayout) {
    auto ptr =
        std::shared_ptr<PipelineLayout>(new PipelineLayout(device, setLayout));
    return ptr;
  }
};

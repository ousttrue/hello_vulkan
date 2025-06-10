#pragma once
#include "vko.h"
#include <functional>
#include <vulkan/vulkan_core.h>

namespace vko {

inline VkRenderPass createSimpleRenderPass(VkDevice device, VkFormat format) {
  VkAttachmentDescription colorAttachments[] = {
      {
          .format = format,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      },
  };

  VkAttachmentReference colorAttachmentRefs[] = {
      {
          .attachment = 0,
          .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      },
  };
  VkSubpassDescription subpasses[] = {
      {
          .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
          .colorAttachmentCount =
              static_cast<uint32_t>(std::size(colorAttachmentRefs)),
          .pColorAttachments = colorAttachmentRefs,
      },
  };

  VkSubpassDependency dependencies[] = {
      {
          .srcSubpass = VK_SUBPASS_EXTERNAL,
          .dstSubpass = 0,
          .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .srcAccessMask = 0,
          .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      },
  };

  VkRenderPassCreateInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      //
      .attachmentCount = static_cast<uint32_t>(std::size(colorAttachments)),
      .pAttachments = colorAttachments,
      //
      .subpassCount = static_cast<uint32_t>(std::size(subpasses)),
      .pSubpasses = subpasses,
      //
      .dependencyCount = static_cast<uint32_t>(std::size(dependencies)),
      .pDependencies = dependencies,
  };

  VkRenderPass renderPass;
  if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) !=
      VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }
  return renderPass;
}

inline VkShaderModule createShaderModule(VkDevice device,
                                         const std::vector<uint32_t> &spv) {
  VkShaderModuleCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = spv.size() * 4,
      .pCode = spv.data(),
  };
  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    Logger::Error("failed to create shader module!");
    return VK_NULL_HANDLE;
  }
  return shaderModule;
}

struct ShaderModule : not_copyable {
  VkDevice device;

  VkShaderModule shaderModule;
  operator VkShaderModule() const { return shaderModule; }

  std::string name;

  VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      // .module = vs,
      // .pName = "main",
  };

  ShaderModule(VkDevice _device, VkShaderModule _shaderModule,
               VkShaderStageFlagBits _stage, const char *_name)
      : device(_device), shaderModule(_shaderModule), name(_name) {
    this->pipelineShaderStageCreateInfo.stage = _stage;
    this->pipelineShaderStageCreateInfo.module = _shaderModule;
    this->pipelineShaderStageCreateInfo.pName = this->name.c_str();
  }

  ~ShaderModule() {
    vkDestroyShaderModule(this->device, this->shaderModule, nullptr);
  }

  static ShaderModule createVertexShader(VkDevice device,
                                         const std::vector<uint32_t> &spv,
                                         const char *entryPoint) {
    auto m = createShaderModule(device, spv);
    return vko::ShaderModule(device, m, VK_SHADER_STAGE_VERTEX_BIT, entryPoint);
  }

  static ShaderModule createFragmentShader(VkDevice device,
                                           const std::vector<uint32_t> &spv,
                                           const char *entryPoint) {
    auto m = createShaderModule(device, spv);
    return vko::ShaderModule(device, m, VK_SHADER_STAGE_FRAGMENT_BIT,
                             entryPoint);
  }
};

struct DescriptorSets : not_copyable {
  VkDevice device;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> descriptorSets;

  DescriptorSets(VkDevice _device,
                 const std::vector<VkDescriptorSetLayoutBinding> &bindings)
      : device(_device) {
    // assert(bindings.size());
    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(std::size(bindings)),
        .pBindings = bindings.data(),
    };
    VKO_CHECK(vkCreateDescriptorSetLayout(this->device, &layoutInfo, nullptr,
                                          &this->descriptorSetLayout));
  }

  ~DescriptorSets() {
    vkDestroyDescriptorPool(this->device, this->descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(this->device, this->descriptorSetLayout,
                                 nullptr);
  }

  void allocate(uint32_t count,
                const std::vector<VkDescriptorPoolSize> &poolSizes) {
    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = count,
        .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
        .pPoolSizes = poolSizes.data(),
    };
    VKO_CHECK(vkCreateDescriptorPool(this->device, &poolInfo, nullptr,
                                     &this->descriptorPool));

    std::vector<VkDescriptorSetLayout> layouts(count, descriptorSetLayout);
    VkDescriptorSetAllocateInfo descriptorAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = this->descriptorPool,
        .descriptorSetCount = count,
        .pSetLayouts = layouts.data(),
    };
    this->descriptorSets.resize(count);
    VKO_CHECK(vkAllocateDescriptorSets(this->device, &descriptorAllocInfo,
                                       this->descriptorSets.data()));
  }

  void update(uint32_t index, VkDescriptorBufferInfo bufferInfo,
              VkDescriptorImageInfo imageInfo) {
    VkWriteDescriptorSet descriptorWrites[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = this->descriptorSets[index],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &bufferInfo,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = this->descriptorSets[index],
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo,
        },
    };
    vkUpdateDescriptorSets(device,
                           static_cast<uint32_t>(std::size(descriptorWrites)),
                           descriptorWrites, 0, nullptr);
  }
};

struct Pipeline : not_copyable {
  VkDevice device;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline graphicsPipeline = VK_NULL_HANDLE;
  Pipeline(VkDevice _device, VkRenderPass _renderPass,
           VkPipelineLayout _pipelineLayout, VkPipeline _graphicsPipeline)
      : device(_device), renderPass(_renderPass),
        pipelineLayout(_pipelineLayout), graphicsPipeline(_graphicsPipeline) {}
  ~Pipeline() {
    vkDestroyPipeline(this->device, this->graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(this->device, this->pipelineLayout, nullptr);
    vkDestroyRenderPass(this->device, this->renderPass, nullptr);
  }
};

struct PipelineBilder {
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .vertexAttributeDescriptionCount = 0,
  };

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  VkPipelineViewportStateCreateInfo viewportState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f,
  };

  VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
  };

  std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = {{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  }};

  Pipeline
  create(VkDevice device, VkRenderPass renderPass,
         VkPipelineLayout pipelineLayout,
         const std::vector<VkPipelineShaderStageCreateInfo> &shaderStages) {

    VkPipelineColorBlendStateCreateInfo colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount =
            static_cast<uint32_t>(std::size(this->colorBlendAttachments)),
        .pAttachments = this->colorBlendAttachments.data(),
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
    };

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates)),
        .pDynamicStates = dynamicStates,
    };

    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = static_cast<uint32_t>(std::size(shaderStages)),
        .pStages = shaderStages.data(),
        .pVertexInputState = &this->vertexInputInfo,
        .pInputAssemblyState = &this->inputAssembly,
        .pViewportState = &this->viewportState,
        .pRasterizationState = &this->rasterizer,
        .pMultisampleState = &this->multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout,
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
    };

    VkPipeline graphicsPipeline;
    VKO_CHECK(vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));

    return {device, renderPass, pipelineLayout, graphicsPipeline};
  }
};

// inline Pipeline createSimpleGraphicsPipeline(VkDevice device, VkFormat
// format,
//                                              const ShaderModule &vs,
//                                              const ShaderModule &fs,
//                                              VkPipelineLayout pipelineLayout)
//                                              {
//
//   auto renderPass = createSimpleRenderPass(device, format);
//
//   return PipelineBilder().create(device, renderPass, pipelineLayout,
//                                  {
//                                      vs.pipelineShaderStageCreateInfo,
//                                      fs.pipelineShaderStageCreateInfo,
//                                  },
//                                  format);
// }

struct CommandBufferRecording : public not_copyable {
  VkCommandBuffer commandBuffer;
  CommandBufferRecording(VkCommandBuffer _commandBuffer,
                         VkRenderPass renderPass, VkFramebuffer framebuffer,
                         VkExtent2D extent, VkClearValue clearColor)
      : commandBuffer(_commandBuffer) {
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    VKO_CHECK(vkBeginCommandBuffer(this->commandBuffer, &beginInfo));

    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {.offset = {0, 0}, .extent = extent},
        .clearValueCount = 1,
        .pClearValues = &clearColor,
    };
    vkCmdBeginRenderPass(this->commandBuffer, &renderPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)extent.width,
        .height = (float)extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(this->commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = extent,
    };
    vkCmdSetScissor(this->commandBuffer, 0, 1, &scissor);
  }
  ~CommandBufferRecording() {
    vkCmdEndRenderPass(this->commandBuffer);
    VKO_CHECK(vkEndCommandBuffer(this->commandBuffer));
  }
  void draw(VkPipeline pipeline, uint32_t vertexCount) {
    vkCmdBindPipeline(this->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline);
    vkCmdDraw(this->commandBuffer, vertexCount, 1, 0, 0);
  }
};

struct DeviceMemory : not_copyable {
  VkDevice device;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  operator VkDeviceMemory() const { return this->memory; }
  // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
  // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
  DeviceMemory(VkDevice _device, VkPhysicalDevice physicalDevice,
               const VkMemoryRequirements &memRequirements,
               VkMemoryPropertyFlags properties)
      : device(_device) {
    VkMemoryAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(
            physicalDevice, memRequirements.memoryTypeBits, properties),
    };
    if (vkAllocateMemory(this->device, &allocateInfo, nullptr, &this->memory) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to allocate vertex buffer memory!");
    }
  }
  ~DeviceMemory() {
    if (this->memory != VK_NULL_HANDLE) {
      vkFreeMemory(this->device, this->memory, nullptr);
    }
  }
  static uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                                 uint32_t typeFilter,
                                 VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i)) &&
          (memProperties.memoryTypes[i].propertyFlags & properties) ==
              properties) {
        return i;
      }
    }
    throw std::runtime_error("failed to find suitable memory type!");
  }
  void assign(const void *p, size_t size) {
    void *data;
    vkMapMemory(this->device, this->memory, 0, size, 0, &data);
    memcpy(data, p, size);
    vkUnmapMemory(this->device, this->memory);
  }
  template <typename T> void assign(const T &src) { assign(&src, sizeof(src)); }
};

struct Buffer : not_copyable {
  VkDevice device;
  VkBuffer buffer;
  std::shared_ptr<DeviceMemory> memory;

  Buffer(VkPhysicalDevice physicalDevice, VkDevice _device, VkDeviceSize size,
         VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
      : device(_device) {

    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VKO_CHECK(
        vkCreateBuffer(this->device, &bufferInfo, nullptr, &this->buffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(this->device, buffer, &memRequirements);
    this->memory = std::make_shared<DeviceMemory>(this->device, physicalDevice,
                                                  memRequirements, properties);

    vkBindBufferMemory(this->device, this->buffer, *this->memory, 0);
  }
  ~Buffer() { vkDestroyBuffer(this->device, this->buffer, nullptr); }
  void copyCommand(VkCommandBuffer commandBuffer, VkBuffer dstBuffer,
                   VkDeviceSize size) {
    VkBufferCopy copyRegion{
        .srcOffset = 0, // optional
        .dstOffset = 0, // optional
        .size = size,
    };
    vkCmdCopyBuffer(commandBuffer, this->buffer, dstBuffer, 1, &copyRegion);
  }
};

struct Image : not_copyable {
  VkDevice device;
  VkImage image;
  std::shared_ptr<DeviceMemory> memory;

  Image(VkPhysicalDevice physicalDevice, VkDevice _device, uint32_t width,
        uint32_t height, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
      : device(_device) {

    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = VK_SHARING_MODE_EXCLUSIVE,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {.width = width, .height = height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VKO_CHECK(vkCreateImage(this->device, &imageInfo, nullptr, &this->image));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(_device, image, &memRequirements);
    this->memory = std::make_shared<DeviceMemory>(this->device, physicalDevice,
                                                  memRequirements, properties);

    vkBindImageMemory(this->device, this->image, *this->memory, 0);
  }
  ~Image() { vkDestroyImage(this->device, this->image, nullptr); }
};

struct CommandPool : not_copyable {
  VkDevice device;
  VkQueue queue;
  VkCommandPool commandPool;
  operator VkCommandPool() const { return this->commandPool; }
  CommandPool(VkDevice _device, uint32_t queueFamilyIndex) : device(_device) {
    vkGetDeviceQueue(this->device, queueFamilyIndex, 0, &this->queue);
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = 0,
        .queueFamilyIndex = queueFamilyIndex,
    };
    VKO_CHECK(vkCreateCommandPool(this->device, &poolInfo, nullptr,
                                  &this->commandPool));
  }
  ~CommandPool() {
    vkDestroyCommandPool(this->device, this->commandPool, nullptr);
  }
};

inline VkCommandBuffer beginSingleTimeCommands(VkDevice device,
                                               VkCommandPool commandPool) {
  VkCommandBufferAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

inline void endSingleTimeCommands(VkDevice device, VkCommandPool commandPool,
                                  VkQueue graphicsQueue,
                                  VkCommandBuffer commandBuffer) {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer,
  };
  vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

  vkQueueWaitIdle(graphicsQueue);

  vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

inline void executeCommandSync(
    VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool,
    const std::function<void(VkCommandBuffer)> &commandRecorder) {
  auto command = beginSingleTimeCommands(device, commandPool);
  commandRecorder(command);
  endSingleTimeCommands(device, commandPool, graphicsQueue, command);
}

} // namespace vko

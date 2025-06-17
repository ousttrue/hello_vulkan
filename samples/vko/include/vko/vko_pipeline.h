#pragma once
#include "vko.h"
#include <functional>
#include <stdio.h>
#include <vulkan/vulkan_core.h>

namespace vko {

inline VkRenderPass createColorRenderPass(VkDevice device, VkFormat format) {
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

inline VkRenderPass createColorDepthRenderPass(VkDevice device,
                                               VkFormat colorFormat,
                                               VkFormat depthFormat) {

  VkAttachmentDescription attachments[] = {
      {
          .format = colorFormat,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      },
      {
          .format = depthFormat,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      },
  };

  VkAttachmentReference colorRef = {0,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkAttachmentReference depthRef = {
      1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpasses[] = {
      {
          .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
          .colorAttachmentCount = 1,
          .pColorAttachments = &colorRef,
          .pDepthStencilAttachment = &depthRef,
      },
  };

  VkRenderPassCreateInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      //
      .attachmentCount = static_cast<uint32_t>(std::size(attachments)),
      .pAttachments = attachments,
      //
      .subpassCount = static_cast<uint32_t>(std::size(subpasses)),
      .pSubpasses = subpasses,
      //
      .dependencyCount = 0, // static_cast<uint32_t>(std::size(dependencies)),
      .pDependencies = nullptr, // dependencies,
  };

  VkRenderPass renderPass;
  if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) !=
      VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }

  //   if (SetDebugUtilsObjectNameEXT(device, VK_OBJECT_TYPE_RENDER_PASS,
  //                                  (uint64_t)ptr->pass,
  //                                  "hello_xr render pass") != VK_SUCCESS) {
  //     throw std::runtime_error("SetDebugUtilsObjectNameEXT");
  //   }

  return renderPass;
}

inline VkPipelineLayout
createPipelineLayoutWithConstantSize(VkDevice device, uint32_t constantSize) {
  VkPushConstantRange pushConstantRanges[] = {
      {
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset = 0,
          .size = constantSize,
      },
  };
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pushConstantRangeCount =
          static_cast<uint32_t>(std::size(pushConstantRanges)),
      .pPushConstantRanges = pushConstantRanges,
  };
  VkPipelineLayout layout;
  VKO_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr,
                                   &layout));
  return layout;
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

inline std::vector<uint32_t> strToUints(const std::string &src) {
  std::vector<uint32_t> uints(src.size() / 4);
  memcpy(uints.data(), src.data(), src.size());
  return uints;
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
  operator VkPipeline() const { return this->graphicsPipeline; }
  Pipeline(VkDevice _device, VkRenderPass _renderPass,
           VkPipelineLayout _pipelineLayout, VkPipeline _graphicsPipeline)
      : device(_device), renderPass(_renderPass),
        pipelineLayout(_pipelineLayout), graphicsPipeline(_graphicsPipeline) {}
  ~Pipeline() {
    vkDestroyPipeline(this->device, this->graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(this->device, this->pipelineLayout, nullptr);
    vkDestroyRenderPass(this->device, this->renderPass, nullptr);
  }
  Pipeline(const Pipeline &) = delete;
  Pipeline &operator=(const Pipeline &) = delete;
  Pipeline(Pipeline &&rhs) {
    this->device = rhs.device;
    this->renderPass = rhs.renderPass;
    rhs.renderPass = VK_NULL_HANDLE;
    this->pipelineLayout = rhs.pipelineLayout;
    rhs.pipelineLayout = VK_NULL_HANDLE;
    this->graphicsPipeline = rhs.graphicsPipeline;
    rhs.graphicsPipeline = VK_NULL_HANDLE;
  }
  Pipeline &operator=(Pipeline &&rhs) {
    this->device = rhs.device;
    this->renderPass = rhs.renderPass;
    rhs.renderPass = VK_NULL_HANDLE;
    this->pipelineLayout = rhs.pipelineLayout;
    rhs.pipelineLayout = VK_NULL_HANDLE;
    this->graphicsPipeline = rhs.graphicsPipeline;
    rhs.graphicsPipeline = VK_NULL_HANDLE;
    return *this;
  }
};

struct PipelineBuilder {
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      // Bindings (spacing between data and if per-instance or per-vertex
      .vertexBindingDescriptionCount = 0,
      // Attribute descriptions (what is passed to vertex shader)
      .vertexAttributeDescriptionCount = 0,
  };

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };
  // VkPipelineInputAssemblyStateCreateInfo ia{
  //     VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  // ia.primitiveRestartEnable = VK_FALSE;
  // ia.topology = this->topology;

  VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f, // optional
      .depthBiasClamp = 0.0f,          // optional
      .depthBiasSlopeFactor = 0.0f,    // optional
      .lineWidth = 1.0f,
  };
  // VkPipelineRasterizationStateCreateInfo rs{
  //     VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  // rs.polygonMode = VK_POLYGON_MODE_FILL;
  // rs.cullMode = VK_CULL_MODE_BACK_BIT;
  // rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
  // rs.depthClampEnable = VK_FALSE;
  // rs.rasterizerDiscardEnable = VK_FALSE;
  // rs.depthBiasEnable = VK_FALSE;
  // rs.depthBiasConstantFactor = 0;
  // rs.depthBiasClamp = 0;
  // rs.depthBiasSlopeFactor = 0;
  // rs.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0f,          // optional
      .pSampleMask = nullptr,            // optional
      .alphaToCoverageEnable = VK_FALSE, // optional
      .alphaToOneEnable = VK_FALSE,      // optional
  };

  std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = {{
      .blendEnable = VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,  // optional
      .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO, // optional
      .colorBlendOp = VK_BLEND_OP_ADD,             // optional
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,  // optional
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, // optional
      .alphaBlendOp = VK_BLEND_OP_ADD,             // optional
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  }};
  // VkPipelineColorBlendAttachmentState attachState{};
  // attachState.blendEnable = 0;
  // attachState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  // attachState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  // attachState.colorBlendOp = VK_BLEND_OP_ADD;
  // attachState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  // attachState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  // attachState.alphaBlendOp = VK_BLEND_OP_ADD;
  // attachState.colorWriteMask =
  //     VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
  //     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .front =
          {
              .failOp = VK_STENCIL_OP_KEEP,
              .passOp = VK_STENCIL_OP_KEEP,
              .depthFailOp = VK_STENCIL_OP_KEEP,
              .compareOp = VK_COMPARE_OP_ALWAYS,
          },
      .back =
          {
              .failOp = VK_STENCIL_OP_KEEP,
              .passOp = VK_STENCIL_OP_KEEP,
              .depthFailOp = VK_STENCIL_OP_KEEP,
              .compareOp = VK_COMPARE_OP_ALWAYS,
          },
      .minDepthBounds = 0.0f,
      .maxDepthBounds = 1.0f,
  };

  Pipeline
  create(VkDevice device, VkRenderPass renderPass,
         VkPipelineLayout pipelineLayout,
         const std::vector<VkPipelineShaderStageCreateInfo> &shaderStages,
         const std::vector<VkVertexInputBindingDescription>
             &vertexInputBindingDescriptions = {},
         const std::vector<VkVertexInputAttributeDescription>
             &attributeDescriptions = {},
         const std::vector<VkViewport> &viewports = {},
         const std::vector<VkRect2D> &scissors = {},
         const std::vector<VkDynamicState> &dynamicStates = {}) {

    this->vertexInputInfo.vertexBindingDescriptionCount =
        static_cast<uint32_t>(std::size(vertexInputBindingDescriptions)),
    this->vertexInputInfo.pVertexBindingDescriptions =
        vertexInputBindingDescriptions.data();

    this->vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(std::size(attributeDescriptions));
    this->vertexInputInfo.pVertexAttributeDescriptions =
        attributeDescriptions.data();

    VkPipelineColorBlendStateCreateInfo colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount =
            static_cast<uint32_t>(std::size(this->colorBlendAttachments)),
        .pAttachments = this->colorBlendAttachments.data(),
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
    };
    // VkPipelineColorBlendStateCreateInfo cb{
    //     VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    // cb.attachmentCount = 1;
    // cb.pAttachments = &attachState;
    // cb.logicOpEnable = VK_FALSE;
    // cb.logicOp = VK_LOGIC_OP_NO_OP;
    // cb.blendConstants[0] = 1.0f;
    // cb.blendConstants[1] = 1.0f;
    // cb.blendConstants[2] = 1.0f;
    // cb.blendConstants[3] = 1.0f;

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = static_cast<uint32_t>(std::size(viewports)),
        .pViewports = viewports.data(),
        .scissorCount = static_cast<uint32_t>(std::size(scissors)),
        .pScissors = scissors.data(),
    };
    // VkPipelineViewportStateCreateInfo vp{
    //     VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    // vp.viewportCount = 1;
    // vp.pViewports = &viewport;
    // vp.scissorCount = 1;
    // vp.pScissors = &scissor;

    // std::vector<VkDynamicState> dynamicStates; //[] =
    // {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates)),
        .pDynamicStates = dynamicStates.data(),
    };
    // VkPipelineDynamicStateCreateInfo dynamicState{
    //     .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    // };
    // std::vector<VkDynamicState> dynamicStateEnables;
    // dynamicState.dynamicStateCount =
    // (uint32_t)this->dynamicStateEnables.size(); dynamicState.pDynamicStates =
    // this->dynamicStateEnables.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = static_cast<uint32_t>(std::size(shaderStages)),
        .pStages = shaderStages.data(),
        .pVertexInputState = &this->vertexInputInfo,
        .pInputAssemblyState = &this->inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &this->rasterizer,
        .pMultisampleState = &this->multisampling,
        .pDepthStencilState = &this->depthStencilStateCreateInfo,
        .pColorBlendState = &colorBlending,
        .pDynamicState = dynamicStates.size() ? &dynamicState : nullptr,
        .layout = pipelineLayout,
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
    };

    // VkGraphicsPipelineCreateInfo pipeInfo{
    //     .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    //     .stageCount = static_cast<uint32_t>(std::size(stages)),
    //     .pStages = stages,
    //     .pVertexInputState = &vi,
    //     .pInputAssemblyState = &ia,
    //     .pTessellationState = nullptr,
    //     .pViewportState = &vp,
    //     .pRasterizationState = &rs,
    //     .pMultisampleState = &ms,
    //     .pDepthStencilState = &ds,
    //     .pColorBlendState = &cb,
    //     .layout = this->pipelineLayout,
    //     .renderPass = this->renderPass,
    //     .subpass = 0,
    // };
    // if (dynamicState.dynamicStateCount > 0) {
    //   pipeInfo.pDynamicState = &dynamicState;
    // }
    // if (vkCreateGraphicsPipelines(this->device, VK_NULL_HANDLE, 1, &pipeInfo,
    //                               nullptr, &this->pipeline) != VK_SUCCESS) {
    //   throw std::runtime_error("vkCreateGraphicsPipelines");
    // }

    VkPipeline graphicsPipeline;
    VKO_CHECK(vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));

    return {device, renderPass, pipelineLayout, graphicsPipeline};
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
// std::shared_ptr<MemoryAllocator>
// MemoryAllocator::Create(VkPhysicalDevice physicalDevice, VkDevice device) {
//   auto ptr = std::shared_ptr<MemoryAllocator>(new MemoryAllocator);
//   ptr->m_vkDevice = device;
//   vkGetPhysicalDeviceMemoryProperties(physicalDevice, &ptr->m_memProps);
//   return ptr;
// }
//
// VkDeviceMemory MemoryAllocator::Allocate(VkMemoryRequirements const &memReqs,
//                                          VkFlags flags,
//                                          const void *pNext) const {
//   // Search memtypes to find first index with those properties
//   for (uint32_t i = 0; i < m_memProps.memoryTypeCount; ++i) {
//     if ((memReqs.memoryTypeBits & (1 << i)) != 0u) {
//       // Type is available, does it match user properties?
//       if ((m_memProps.memoryTypes[i].propertyFlags & flags) == flags) {
//         VkMemoryAllocateInfo memAlloc{
//             .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
//             .pNext = pNext,
//             .allocationSize = memReqs.size,
//             .memoryTypeIndex = i,
//         };
//         VkDeviceMemory mem;
//         if (vkAllocateMemory(m_vkDevice, &memAlloc, nullptr, &mem) !=
//             VK_SUCCESS) {
//           throw std::runtime_error("vkAllocateMemory");
//         }
//         return mem;
//       }
//     }
//   }
//   throw std::runtime_error("Memory format not supported");
// }
//
// VkDeviceMemory MemoryAllocator::Allocate(VkImage image, VkFlags flags) const
// {
//   VkMemoryRequirements memRequirements;
//   vkGetImageMemoryRequirements(m_vkDevice, image, &memRequirements);
//   return Allocate(memRequirements, flags);
// }
//
// VkDeviceMemory MemoryAllocator::AllocateBufferMemory(VkBuffer buf) const {
//   VkMemoryRequirements memReq = {};
//   vkGetBufferMemoryRequirements(m_vkDevice, buf, &memReq);
//   return Allocate(memReq);
// }

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

struct IndexedMesh {
  std::shared_ptr<vko::Buffer> vertexBuffer;
  std::vector<VkVertexInputBindingDescription> inputBindingDescriptions;
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
  // describes position
  // {
  //     .location = 0,
  //     .binding = 0,
  //     .format = VK_FORMAT_R32G32_SFLOAT,
  //     .offset = offsetof(Vertex, pos),
  // },
  // // describes color
  // {
  //     .location = 1,
  //     .binding = 0,
  //     .format = VK_FORMAT_R32G32B32_SFLOAT,
  //     .offset = offsetof(Vertex, color),
  // },
  // // uv
  // {
  //     .location = 2,
  //     .binding = 0,
  //     .format = VK_FORMAT_R32G32_SFLOAT,
  //     .offset = offsetof(Vertex, texCoord),
  // },

  std::shared_ptr<vko::Buffer> indexBuffer;
  uint32_t indexDrawCount = 0;
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

struct DepthImage : not_copyable {
  VkDevice device;
  VkImage image = VK_NULL_HANDLE;
  std::shared_ptr<DeviceMemory> memory;
  VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

  // VkDeviceMemory depthMemory{VK_NULL_HANDLE};
  // VkDevice m_vkDevice{VK_NULL_HANDLE};
  // VkImage depthImage{VK_NULL_HANDLE};

  DepthImage(VkDevice _device, VkPhysicalDevice physicalDevice, VkExtent2D size,
             VkFormat depthFormat, VkSampleCountFlagBits sampleCount,
             VkMemoryPropertyFlags properties)
      // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
      : device(_device) {

    // Create a D32 depthbuffer
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depthFormat,
        .extent =
            {
                .width = size.width,
                .height = size.height,
                .depth = 1,
            },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = sampleCount,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VKO_CHECK(vkCreateImage(this->device, &imageInfo, nullptr, &this->image));
    // if (SetDebugUtilsObjectNameEXT(
    //         device, VK_OBJECT_TYPE_IMAGE, (uint64_t)ptr->depthImage,
    //         "hello_xr fallback depth image") != VK_SUCCESS) {
    //   throw std::runtime_error("SetDebugUtilsObjectNameEXT");
    // }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(_device, image, &memRequirements);
    this->memory = std::make_shared<DeviceMemory>(this->device, physicalDevice,
                                                  memRequirements, properties);

    // auto memAllocator = MemoryAllocator::Create(physicalDevice, device);
    // ptr->depthMemory = memAllocator->Allocate( ptr->depthImage,
    // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); if (SetDebugUtilsObjectNameEXT(
    //         device, VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)ptr->depthMemory,
    //         "hello_xr fallback depth image memory") != VK_SUCCESS) {
    //   throw std::runtime_error("SetDebugUtilsObjectNameEXT");
    // }
    VKO_CHECK(vkBindImageMemory(this->device, this->image, *this->memory, 0));
  }
  ~DepthImage() { vkDestroyImage(this->device, this->image, nullptr); }

  void TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout) {
    if (newLayout != this->layout) {
      VkImageMemoryBarrier depthBarrier{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
          .oldLayout = this->layout,
          .newLayout = newLayout,
          .image = this->image,
          .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
      };
      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                           VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0,
                           nullptr, 1, &depthBarrier);
      this->layout = newLayout;
    }
  }
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

struct CommandBufferRecording : public not_copyable {
  VkCommandBuffer commandBuffer;
  CommandBufferRecording(VkCommandBuffer _commandBuffer,
                         VkRenderPass renderPass, VkPipeline pipeline,
                         VkFramebuffer framebuffer, VkExtent2D extent,
                         VkClearValue clearColor,
                         VkPipelineLayout pipelineLayout = VK_NULL_HANDLE,
                         VkDescriptorSet descriptorSet = VK_NULL_HANDLE)
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

    vkCmdBindPipeline(this->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline);

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

    if (pipelineLayout != VK_NULL_HANDLE && descriptorSet != VK_NULL_HANDLE) {
      // take the descriptor set for the corresponding swap image, and bind it
      // to the descriptors in the shader
      vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    }
  }
  ~CommandBufferRecording() {
    vkCmdEndRenderPass(this->commandBuffer);
    VKO_CHECK(vkEndCommandBuffer(this->commandBuffer));
  }
  void draw(uint32_t vertexCount) {
    vkCmdDraw(this->commandBuffer, vertexCount, 1, 0, 0);
  }

  void drawIndexed(const IndexedMesh &mesh) {
    if (mesh.vertexBuffer->buffer != VK_NULL_HANDLE) {
      VkBuffer vertexBuffers[] = {mesh.vertexBuffer->buffer};
      VkDeviceSize offsets[] = {0};
      vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    }
    if (mesh.indexBuffer->buffer != VK_NULL_HANDLE) {
      vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer->buffer, 0,
                           VK_INDEX_TYPE_UINT16);
    }
    vkCmdDrawIndexed(commandBuffer, mesh.indexDrawCount, 1, 0, 0, 0);
  }
};

inline void copyBytesToBufferCommand(VkPhysicalDevice physicalDevice,
                                     VkDevice device,
                                     uint32_t graphicsQueueFamilyIndex,
                                     const void *pSrc, uint32_t copySize,
                                     VkBuffer dst) {
  auto stagingBuffer = std::make_shared<vko::Buffer>(
      physicalDevice, device, copySize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  stagingBuffer->memory->assign(pSrc, copySize);
  vko::CommandPool commandPool(device, graphicsQueueFamilyIndex);
  vko::executeCommandSync(device, commandPool.queue, commandPool,
                          [stagingBuffer, dst, copySize](auto commandBuffer) {
                            stagingBuffer->copyCommand(commandBuffer, dst,
                                                       copySize);
                          });
}

} // namespace vko

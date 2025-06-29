#pragma once
#include "../vk.h"
#include <span>

namespace vuloxr {

namespace vk {

inline VkPipelineLayout createEmptyPipelineLayout(VkDevice device) {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 0,
      .pushConstantRangeCount = 0,
  };
  VkPipelineLayout pipelineLayout;
  CheckVkResult(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                       &pipelineLayout));
  return pipelineLayout;
}

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
          //     .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
          //                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
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

struct Pipeline : NonCopyable {
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
      .flags = 0,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      //     .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f, // optional
      .depthBiasClamp = 0.0f,          // optional
      .depthBiasSlopeFactor = 0.0f,    // optional
      .lineWidth = 1.0f,
  };

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

  VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfoEnabled{
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
  VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfoDisabled = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_FALSE,
      .depthWriteEnable = VK_FALSE,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
  };

  Pipeline create(
      VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout,
      const std::vector<VkPipelineShaderStageCreateInfo> &shaderStages,
      std::span<const VkVertexInputBindingDescription>
          vertexInputBindingDescriptions = {},
      std::span<const VkVertexInputAttributeDescription> attributeDescriptions =
          {},
      const std::vector<VkViewport> &viewports = {},
      const std::vector<VkRect2D> &scissors = {},
      // {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
      const std::vector<VkDynamicState> &dynamicStates = {}) {

    for (auto dynamic : dynamicStates) {
      Logger::Info("[dynamic] %s", magic_enum::enum_name(dynamic).data());
    }

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
    };
    if (viewports.empty()) {
      // https://github.com/KhronosGroup/Vulkan-Guide/blob/main/lang/jp/chapters/dynamic_state.adoc
      viewportState.viewportCount = 1;
      viewportState.pViewports = nullptr;
    } else {
      viewportState.viewportCount = static_cast<uint32_t>(std::size(viewports));
      viewportState.pViewports = viewports.data();
    }
    if (scissors.empty()) {
      viewportState.scissorCount = 1;
      viewportState.pScissors = nullptr;
    } else {
      viewportState.scissorCount = static_cast<uint32_t>(std::size(scissors));
      viewportState.pScissors = scissors.data();
    };

    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates)),
        .pDynamicStates = dynamicStates.data(),
    };

    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = static_cast<uint32_t>(std::size(shaderStages)),
        .pStages = shaderStages.data(),
        .pVertexInputState = &this->vertexInputInfo,
        .pInputAssemblyState = &this->inputAssembly,
        .pTessellationState = nullptr,
        .pViewportState =
            viewportState.viewportCount > 0 ? &viewportState : nullptr,
        .pRasterizationState = &this->rasterizer,
        .pMultisampleState = &this->multisampling,
        .pDepthStencilState = &this->depthStencilStateCreateInfoDisabled,
        .pColorBlendState = &colorBlending,
        .pDynamicState = dynamicStates.size() ? &dynamicState : nullptr,
        .layout = pipelineLayout,
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
    };

    VkPipeline pipeline;
    CheckVkResult(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                            &pipelineInfo, nullptr, &pipeline));

    return {device, renderPass, pipelineLayout, pipeline};
  }
};

inline VkShaderModule createShaderModule(VkDevice device,
                                         const std::vector<uint32_t> &spv) {
  VkShaderModuleCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = spv.size() * 4,
      .pCode = spv.data(),
  };
  VkShaderModule shaderModule = VK_NULL_HANDLE;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    Logger::Error("failed to create shader module!");
    return VK_NULL_HANDLE;
  }
  return shaderModule;
}

struct ShaderModule : NonCopyable {
  VkDevice device = VK_NULL_HANDLE;

  VkShaderModule shaderModule = VK_NULL_HANDLE;
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
    if (this->shaderModule != VK_NULL_HANDLE) {
      vkDestroyShaderModule(this->device, this->shaderModule, nullptr);
    }
  }

  static ShaderModule createVertexShader(VkDevice device,
                                         const std::vector<uint32_t> &spv,
                                         const char *entryPoint) {
    auto m = createShaderModule(device, spv);
    return ShaderModule(device, m, VK_SHADER_STAGE_VERTEX_BIT, entryPoint);
  }

  static ShaderModule createFragmentShader(VkDevice device,
                                           const std::vector<uint32_t> &spv,
                                           const char *entryPoint) {
    auto m = createShaderModule(device, spv);
    return ShaderModule(device, m, VK_SHADER_STAGE_FRAGMENT_BIT, entryPoint);
  }
};

struct DescriptorSets : NonCopyable {
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
    CheckVkResult(vkCreateDescriptorSetLayout(
        this->device, &layoutInfo, nullptr, &this->descriptorSetLayout));
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
    CheckVkResult(vkCreateDescriptorPool(this->device, &poolInfo, nullptr,
                                         &this->descriptorPool));

    std::vector<VkDescriptorSetLayout> layouts(count, descriptorSetLayout);
    VkDescriptorSetAllocateInfo descriptorAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = this->descriptorPool,
        .descriptorSetCount = count,
        .pSetLayouts = layouts.data(),
    };
    this->descriptorSets.resize(count);
    CheckVkResult(vkAllocateDescriptorSets(this->device, &descriptorAllocInfo,
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

  VkSubpassDependency dependencies[] = {
      {
          .srcSubpass = VK_SUBPASS_EXTERNAL,
          .dstSubpass = 0,
          .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .srcAccessMask = 0,
          .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          //     .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
          //                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
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
  CheckVkResult(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo,
                                       nullptr, &layout));
  return layout;
}

} // namespace vk
} // namespace vuloxr

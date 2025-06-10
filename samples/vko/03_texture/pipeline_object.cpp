#include "pipeline_object.h"
#include "../glsl_to_spv.h"
#include "vko/vko.h"
#include <array>
#include <stdexcept>
#include <vko/vko_pipeline.h>
#include <vulkan/vulkan_core.h>

auto VS = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main()
{
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0); 
    fragColor = inColor; 
    fragTexCoord = inTexCoord;
}
)";

auto FS = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(texSampler, fragTexCoord);
}
)";

PipelineObject::PipelineObject(
    VkPhysicalDevice physicalDevice, VkDevice _device,
    //
    VkFormat swapchainFormat, VkExtent2D swapchainExtent,
    //
    VkDescriptorSetLayout descriptorSetLayout,
    const VkVertexInputBindingDescription &vertexInputBindingDescription,
    const std::vector<VkVertexInputAttributeDescription> &attributeDescriptions)
    : device(_device) {

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptorSetLayout,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };
  VKO_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                   &this->pipelineLayout));

  this->renderPass = vko::createSimpleRenderPass(device, swapchainFormat);

  // auto vertShaderCode = readFile("shaders/vert.spv");
  VkShaderModule vertShaderModule =
      vko::createShaderModule(_device, glsl_vs_to_spv(VS));
  // auto fragShaderCode = readFile("shaders/frag.spv");
  VkShaderModule fragShaderModule =
      vko::createShaderModule(_device, glsl_fs_to_spv(FS));
  VkPipelineShaderStageCreateInfo shaderStages[] = {
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = vertShaderModule,
          .pName = "main",
      },
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = fragShaderModule,
          .pName = "main",
      },
  };

  // This struct decribes the format of the vertex data:
  //    1. Bindings (spacing between data and if per-instance or per-vertex
  //    2. Attribute descriptions (what is passed to vertex shader)
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vertexInputBindingDescription,
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(std::size(attributeDescriptions)),
      .pVertexAttributeDescriptions = attributeDescriptions.data(),
  };

  // This struct describes two things as well:
  //    1. Type of geometry drawn from vertices
  //    2. If primitive restart should be enabled
  VkPipelineInputAssemblyStateCreateInfo inputAssembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = (float)swapchainExtent.width,
      .height = (float)swapchainExtent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  VkRect2D scissor{
      .offset = {0, 0},
      .extent = swapchainExtent,
  };
  VkPipelineViewportStateCreateInfo viewportState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor,
  };

  // build the rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      // counter clockwise (to match Y-flip in the projection matrix)
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f, // optional
      .depthBiasClamp = 0.0f,          // optional
      .depthBiasSlopeFactor = 0.0f,    // optional
      .lineWidth = 1.0f,
  };

  // Multisampling
  VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0f,          // optional
      .pSampleMask = nullptr,            // optional
      .alphaToCoverageEnable = VK_FALSE, // optional
      .alphaToOneEnable = VK_FALSE,      // optional
  };

  // Color Blending
  VkPipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,  // optional
      .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO, // optional
      .colorBlendOp = VK_BLEND_OP_ADD,             // optional
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,  // optional
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, // optional
      .alphaBlendOp = VK_BLEND_OP_ADD,             // optional
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };
  // references the array of structures for all framebuffers and allows for
  // blend factor specification
  VkPipelineColorBlendStateCreateInfo colorBlending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY, // optional
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment,
      .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
  };

  VkGraphicsPipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = shaderStages,
      .pVertexInputState = &vertexInputInfo,
      .pInputAssemblyState = &inputAssembly,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = nullptr,
      .pColorBlendState = &colorBlending,
      .pDynamicState = nullptr,
      .layout = this->pipelineLayout,
      .renderPass = this->renderPass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
  };
  if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr,
                                &this->graphicsPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
  vkDestroyShaderModule(_device, fragShaderModule, nullptr);
  vkDestroyShaderModule(_device, vertShaderModule, nullptr);
}

PipelineObject::~PipelineObject() {
  vkDestroyPipeline(this->device, this->graphicsPipeline, nullptr);
  vkDestroyRenderPass(this->device, this->renderPass, nullptr);
  vkDestroyPipelineLayout(this->device, this->pipelineLayout, nullptr);
}

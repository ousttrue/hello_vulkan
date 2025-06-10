#include "pipeline_object.h"
#include "../glsl_to_spv.h"
#include "scene.h"
#include "vko/vko.h"
#include <array>
#include <fstream>
#include <functional>
#include <glm/fwd.hpp>
#include <memory>
#include <stdexcept>
#include <vko/vko_pipeline.h>
#include <vulkan/vulkan_core.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> //<-- includes functions such as ones that allow rotation, and perspective

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

void UniformBufferObject::setTime(float time, float width, float height) {
  // time * radians(90.0f) = rotation of 90 degrees per second!
  *((glm::mat4 *)&this->model) = glm::rotate(
      glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  // specify view position
  *((glm::mat4 *)&this->view) =
      glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f));
  // view from a 45 degree angle
  *((glm::mat4 *)&this->proj) =
      glm::perspective(glm::radians(45.0f), width / height, 1.0f, 10.0f);

  // GLM was designed for OpenGL, therefor Y-coordinate (of the clip
  // coodinates) is inverted, the following code flips this around!
  this->proj.m[5] /*[1][1]*/ *= -1;
}

static VkShaderModule createShaderModule(VkDevice device,
                                         const std::vector<uint32_t> &code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size() * 4;
  createInfo.pCode = code.data();

  VkShaderModule shaderModule;

  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create shader module!");
  }

  return shaderModule;
}

PipelineObject::PipelineObject(VkPhysicalDevice physicalDevice, VkDevice device,
                               uint32_t graphicsQueueFamilyIndex,
                               VkFormat swapchainFormat,
                               VkDescriptorSetLayout descriptorSetLayout)
    : _device(device), _descriptorSetLayout(descriptorSetLayout) {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &_descriptorSetLayout,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };
  VKO_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                   &this->_pipelineLayout));

  this->_renderPass = vko::createSimpleRenderPass(device, swapchainFormat);

  vko::CommandPool commandPool(device, graphicsQueueFamilyIndex);
}

PipelineObject::~PipelineObject() {
  vkDestroyRenderPass(_device, _renderPass, nullptr);

  if (_graphicsPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
    _graphicsPipeline = VK_NULL_HANDLE;
  }

  vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
}

void PipelineObject::createGraphicsPipeline(const Scene &scene,
                                            VkExtent2D swapchainExtent) {
  if (_graphicsPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
    _graphicsPipeline = VK_NULL_HANDLE;
  }

  // auto vertShaderCode = readFile("shaders/vert.spv");
  VkShaderModule vertShaderModule =
      createShaderModule(_device, glsl_vs_to_spv(VS));
  // auto fragShaderCode = readFile("shaders/frag.spv");
  VkShaderModule fragShaderModule =
      createShaderModule(_device, glsl_fs_to_spv(FS));
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

  // auto bindingDescription = Vertex_getBindingDescription();
  // auto attributeDescriptions = Vertex_getAttributeDescriptions();
  VkVertexInputAttributeDescription attributeDescriptions[]{
      // describes position
      {
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = offsetof(Vertex, pos),
      },
      // describes color
      {
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = offsetof(Vertex, color),
      },
      // uv
      {
          .location = 2,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = offsetof(Vertex, texCoord),
      },
  };

  // This struct decribes the format of the vertex data:
  //    1. Bindings (spacing between data and if per-instance or per-vertex
  //    2. Attribute descriptions (what is passed to vertex shader)
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &scene.vertexInputBindingDescription,
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(std::size(attributeDescriptions)),
      .pVertexAttributeDescriptions = attributeDescriptions,
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
      .layout = _pipelineLayout,
      .renderPass = _renderPass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
  };
  if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &_graphicsPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
  vkDestroyShaderModule(_device, fragShaderModule, nullptr);
  vkDestroyShaderModule(_device, vertShaderModule, nullptr);
}

void PipelineObject::record(VkCommandBuffer commandBuffer,
                            VkFramebuffer framebuffer, VkExtent2D extent,
                            VkDescriptorSet descriptorSet, const Scene &scene) {

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = 0,
      .pInheritanceInfo = nullptr,
  };
  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }

  VkRenderPassBeginInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = _renderPass,
      .framebuffer = framebuffer,
      .renderArea =
          {
              .offset = {0, 0},
              .extent = extent,
          },
      .clearValueCount = 1,
      .pClearValues = &_clearColor,
  };
  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _graphicsPipeline);

  VkBuffer vertexBuffers[] = {scene.vertexBuffer->buffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
  vkCmdBindIndexBuffer(commandBuffer, scene.indexBuffer->buffer, 0,
                       VK_INDEX_TYPE_UINT16);

  // take the descriptor set for the corresponding swap image, and bind it
  // to the descriptors in the shader
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          _pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

  // Tell Vulkan to draw the triangle USING THE INDEX BUFFER!
  vkCmdDrawIndexed(commandBuffer, scene.indexDrawCount, 1, 0, 0, 0);

  vkCmdEndRenderPass(commandBuffer);

  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }
}

VkRenderPass PipelineObject::renderPass() const { return _renderPass; }

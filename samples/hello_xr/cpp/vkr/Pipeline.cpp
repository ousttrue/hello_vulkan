#include "Pipeline.h"
#include "VertexBuffer.h"
#include "glsl_to_spv.h"
#include <array>
#include <common/fmt.h>
#include <common/logger.h>
#include <shaderc/shaderc.hpp>
#include <stdexcept>
#include <vko/vko_pipeline.h>
#include <vulkan/vulkan_core.h>

constexpr char VertexShaderGlsl[] = {
#embed "shader.vert"
};

constexpr char FragmentShaderGlsl[] = {
#embed "shader.frag"
};

//
// Pipeline wrapper for rendering pipeline state
//
Pipeline::~Pipeline() {
  if (m_vkDevice != nullptr) {
    if (m_pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(m_vkDevice, m_pipeline, nullptr);
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(m_vkDevice, m_pipelineLayout, nullptr);
    }
    if (m_renderPass != VK_NULL_HANDLE) {
      vkDestroyRenderPass(m_vkDevice, m_renderPass, nullptr);
    }
  }
  m_pipeline = VK_NULL_HANDLE;
  m_renderPass = VK_NULL_HANDLE;
  m_pipelineLayout = VK_NULL_HANDLE;
  m_vkDevice = nullptr;
}

std::shared_ptr<Pipeline>
Pipeline::Create(VkDevice device, VkExtent2D size, VkFormat colorFormat,
                 VkFormat depthFormat,
                 const std::shared_ptr<VertexBuffer> &vb) {

  auto ptr = std::shared_ptr<Pipeline>(new Pipeline);

  ptr->m_pipelineLayout =
      vko::createPipelineLayoutWithConstantSize(device, sizeof(float) * 16);
  ptr->m_renderPass =
      vko::createColorDepthRenderPass(device, colorFormat, depthFormat);

  ptr->m_vkDevice = device;

  VkPipelineDynamicStateCreateInfo dynamicState{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamicState.dynamicStateCount = (uint32_t)ptr->dynamicStateEnables.size();
  dynamicState.pDynamicStates = ptr->dynamicStateEnables.data();

  VkPipelineVertexInputStateCreateInfo vi{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vi.vertexBindingDescriptionCount = 1;
  vi.pVertexBindingDescriptions = &vb->bindDesc;
  vi.vertexAttributeDescriptionCount = (uint32_t)vb->attrDesc.size();
  vi.pVertexAttributeDescriptions = vb->attrDesc.data();

  VkPipelineInputAssemblyStateCreateInfo ia{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ia.primitiveRestartEnable = VK_FALSE;
  ia.topology = ptr->topology;

  VkPipelineRasterizationStateCreateInfo rs{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_BACK_BIT;
  rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rs.depthClampEnable = VK_FALSE;
  rs.rasterizerDiscardEnable = VK_FALSE;
  rs.depthBiasEnable = VK_FALSE;
  rs.depthBiasConstantFactor = 0;
  rs.depthBiasClamp = 0;
  rs.depthBiasSlopeFactor = 0;
  rs.lineWidth = 1.0f;

  VkPipelineColorBlendAttachmentState attachState{};
  attachState.blendEnable = 0;
  attachState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  attachState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  attachState.colorBlendOp = VK_BLEND_OP_ADD;
  attachState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  attachState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  attachState.alphaBlendOp = VK_BLEND_OP_ADD;
  attachState.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo cb{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  cb.attachmentCount = 1;
  cb.pAttachments = &attachState;
  cb.logicOpEnable = VK_FALSE;
  cb.logicOp = VK_LOGIC_OP_NO_OP;
  cb.blendConstants[0] = 1.0f;
  cb.blendConstants[1] = 1.0f;
  cb.blendConstants[2] = 1.0f;
  cb.blendConstants[3] = 1.0f;

  VkRect2D scissor = {{0, 0}, size};
#if defined(ORIGIN_BOTTOM_LEFT)
  // Flipped view so origin is bottom-left like GL (requires
  // VK_KHR_maintenance1)
  VkViewport viewport = {
      0.0f, (float)size.height, (float)size.width, -(float)size.height, 0.0f,
      1.0f};
#else
  // Will invert y after projection
  VkViewport viewport = {0.0f, 0.0f, (float)size.width, (float)size.height,
                         0.0f, 1.0f};
#endif
  VkPipelineViewportStateCreateInfo vp{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  vp.viewportCount = 1;
  vp.pViewports = &viewport;
  vp.scissorCount = 1;
  vp.pScissors = &scissor;

  VkPipelineDepthStencilStateCreateInfo ds{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  ds.depthTestEnable = VK_TRUE;
  ds.depthWriteEnable = VK_TRUE;
  ds.depthCompareOp = VK_COMPARE_OP_LESS;
  ds.depthBoundsTestEnable = VK_FALSE;
  ds.stencilTestEnable = VK_FALSE;
  ds.front.failOp = VK_STENCIL_OP_KEEP;
  ds.front.passOp = VK_STENCIL_OP_KEEP;
  ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
  ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
  ds.back = ds.front;
  ds.minDepthBounds = 0.0f;
  ds.maxDepthBounds = 1.0f;

  VkPipelineMultisampleStateCreateInfo ms{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  auto vertexSPIRV = glsl_vs_to_spv(VertexShaderGlsl);
  auto vs = vko::ShaderModule::createVertexShader(device, vertexSPIRV, "main");

  auto fragmentSPIRV = glsl_fs_to_spv(FragmentShaderGlsl);
  auto fs =
      vko::ShaderModule::createFragmentShader(device, fragmentSPIRV, "main");

  VkPipelineShaderStageCreateInfo stages[] = {
      vs.pipelineShaderStageCreateInfo,
      fs.pipelineShaderStageCreateInfo,
  };

  VkGraphicsPipelineCreateInfo pipeInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = static_cast<uint32_t>(std::size(stages)),
      .pStages = stages,
      .pVertexInputState = &vi,
      .pInputAssemblyState = &ia,
      .pTessellationState = nullptr,
      .pViewportState = &vp,
      .pRasterizationState = &rs,
      .pMultisampleState = &ms,
      .pDepthStencilState = &ds,
      .pColorBlendState = &cb,
      .layout = ptr->m_pipelineLayout,
      .renderPass = ptr->m_renderPass,
      .subpass = 0,
  };
  if (dynamicState.dynamicStateCount > 0) {
    pipeInfo.pDynamicState = &dynamicState;
  }
  if (vkCreateGraphicsPipelines(ptr->m_vkDevice, VK_NULL_HANDLE, 1, &pipeInfo,
                                nullptr, &ptr->m_pipeline) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateGraphicsPipelines");
  }

  return ptr;
}

#include "Pipeline.h"
#include "VertexBuffer.h"
#include <array>
#include <common/fmt.h>
#include <common/logger.h>
#include <shaderc/shaderc.hpp>
#include <stdexcept>
#include <vko/vko_pipeline.h>
#include <vulkan/vulkan_core.h>

// ShaderProgram to hold a pair of vertex & fragment shaders
class ShaderProgram {
  VkDevice m_vkDevice{VK_NULL_HANDLE};
  void Load(uint32_t index, const std::vector<uint32_t> &code);

  ShaderProgram() = default;

public:
  std::array<VkPipelineShaderStageCreateInfo, 2> shaderInfo{
      {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
       {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}}};
  ~ShaderProgram();
  ShaderProgram(const ShaderProgram &) = delete;
  ShaderProgram &operator=(const ShaderProgram &) = delete;
  ShaderProgram(ShaderProgram &&) = delete;
  ShaderProgram &operator=(ShaderProgram &&) = delete;
  static std::shared_ptr<ShaderProgram> Create(VkDevice device) {
    auto ptr = std::shared_ptr<ShaderProgram>(new ShaderProgram);
    ptr->m_vkDevice = device;
    return ptr;
  }
  void LoadVertexShader(const std::vector<uint32_t> &code) { Load(0, code); }
  void LoadFragmentShader(const std::vector<uint32_t> &code) { Load(1, code); }
};

// Compile a shader to a SPIR-V binary.
static std::vector<uint32_t> CompileGlslShader(const std::string &name,
                                               shaderc_shader_kind kind,
                                               const std::string &source) {
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;

  options.SetOptimizationLevel(shaderc_optimization_level_size);

  shaderc::SpvCompilationResult module =
      compiler.CompileGlslToSpv(source, kind, name.c_str(), options);

  if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
    Log::Write(Log::Level::Error,
               Fmt("Shader %s compilation failed: %s", name.c_str(),
                   module.GetErrorMessage().c_str()));
    return std::vector<uint32_t>();
  }

  return {module.cbegin(), module.cend()};
}

constexpr char VertexShaderGlsl[] = R"_(
#version 430
#extension GL_ARB_separate_shader_objects : enable

layout (std140, push_constant) uniform buf
{
    mat4 mvp;
} ubuf;

layout (location = 0) in vec3 Position;
layout (location = 1) in vec4 Color;

layout (location = 0) out vec4 oColor;
out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    oColor.rgba  = Color.rgba;
    gl_Position = ubuf.mvp * vec4(Position, 1);
}
)_";

constexpr char FragmentShaderGlsl[] = R"_(
#version 430
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec4 oColor;

layout (location = 0) out vec4 FragColor;

void main()
{
    FragColor = oColor;
}
)_";

// glslangValidator doesn't wrap its output in brackets if you don't have it
// define the whole array.
#if defined(USE_GLSLANGVALIDATOR)
#define SPV_PREFIX {
#define SPV_SUFFIX }
#else
#define SPV_PREFIX
#define SPV_SUFFIX
#endif

//
// ShaderProgram to hold a pair of vertex & fragment shaders
//
ShaderProgram::~ShaderProgram() {
  for (auto &si : shaderInfo) {
    if (si.module != VK_NULL_HANDLE) {
      vkDestroyShaderModule(m_vkDevice, si.module, nullptr);
    }
    si.module = VK_NULL_HANDLE;
  }
}

void ShaderProgram::Load(uint32_t index, const std::vector<uint32_t> &code) {
  VkShaderModuleCreateInfo modInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};

  auto &si = shaderInfo[index];
  si.pName = "main";
  std::string name;

  switch (index) {
  case 0:
    si.stage = VK_SHADER_STAGE_VERTEX_BIT;
    name = "vertex";
    break;
  case 1:
    si.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    name = "fragment";
    break;
  default:
    throw std::runtime_error(Fmt("Unknown code index %d", index));
  }

  modInfo.codeSize = code.size() * sizeof(code[0]);
  modInfo.pCode = &code[0];
  if (!(modInfo.codeSize > 0 && modInfo.pCode)) {
    throw std::runtime_error(Fmt("Invalid %s shader ", name.c_str()));
  }

  if (vkCreateShaderModule(m_vkDevice, &modInfo, nullptr, &si.module) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateShaderModule");
  }

  Log::Write(Log::Level::Info, Fmt("Loaded %s shader", name.c_str()));
}

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

  auto vertexSPIRV = CompileGlslShader(
      "vertex", shaderc_glsl_default_vertex_shader, VertexShaderGlsl);
  auto fragmentSPIRV = CompileGlslShader(
      "fragment", shaderc_glsl_default_fragment_shader, FragmentShaderGlsl);
  if (vertexSPIRV.empty()) {
    throw std::runtime_error("Failed to compile vertex shader");
  }
  if (fragmentSPIRV.empty()) {
    throw std::runtime_error("Failed to compile fragment shader");
  }

  auto shaderProgram = ShaderProgram::Create(device);
  shaderProgram->LoadVertexShader(vertexSPIRV);
  shaderProgram->LoadFragmentShader(fragmentSPIRV);

  VkGraphicsPipelineCreateInfo pipeInfo{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pipeInfo.stageCount = (uint32_t)shaderProgram->shaderInfo.size();
  pipeInfo.pStages = shaderProgram->shaderInfo.data();
  pipeInfo.pVertexInputState = &vi;
  pipeInfo.pInputAssemblyState = &ia;
  pipeInfo.pTessellationState = nullptr;
  pipeInfo.pViewportState = &vp;
  pipeInfo.pRasterizationState = &rs;
  pipeInfo.pMultisampleState = &ms;
  pipeInfo.pDepthStencilState = &ds;
  pipeInfo.pColorBlendState = &cb;
  if (dynamicState.dynamicStateCount > 0) {
    pipeInfo.pDynamicState = &dynamicState;
  }
  pipeInfo.layout = ptr->m_pipelineLayout;
  pipeInfo.renderPass = ptr->m_renderPass;
  pipeInfo.subpass = 0;
  if (vkCreateGraphicsPipelines(ptr->m_vkDevice, VK_NULL_HANDLE, 1, &pipeInfo,
                                nullptr, &ptr->m_pipeline) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateGraphicsPipelines");
  }

  return ptr;
}

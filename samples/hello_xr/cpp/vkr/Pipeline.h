#pragma once
#include <array>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

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

// Pipeline wrapper for rendering pipeline state
class Pipeline {
  VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  std::vector<VkDynamicState> dynamicStateEnables;
  VkDevice m_vkDevice{VK_NULL_HANDLE};

  Pipeline() {}

public:
  VkPipeline pipe{VK_NULL_HANDLE};
  static std::shared_ptr<Pipeline>
  Create(VkDevice device, VkExtent2D size, VkPipelineLayout layout,
         VkRenderPass renderPass,
         const std::shared_ptr<class ShaderProgram> &sp,
         const std::shared_ptr<struct VertexBuffer> &vb);

  void Dynamic(VkDynamicState state);
  void Release();
};

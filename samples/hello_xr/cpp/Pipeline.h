#pragma once
#include <array>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

// ShaderProgram to hold a pair of vertex & fragment shaders
struct ShaderProgram {
  std::array<VkPipelineShaderStageCreateInfo, 2> shaderInfo{
      {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
       {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}}};

  ShaderProgram() = default;
  ~ShaderProgram();
  ShaderProgram(const ShaderProgram &) = delete;
  ShaderProgram &operator=(const ShaderProgram &) = delete;
  ShaderProgram(ShaderProgram &&) = delete;
  ShaderProgram &operator=(ShaderProgram &&) = delete;
  void LoadVertexShader(const std::vector<uint32_t> &code) { Load(0, code); }
  void LoadFragmentShader(const std::vector<uint32_t> &code) { Load(1, code); }
  void Init(VkDevice device) { m_vkDevice = device; }

private:
  VkDevice m_vkDevice{VK_NULL_HANDLE};
  void Load(uint32_t index, const std::vector<uint32_t> &code);
};

// Simple vertex MVP xform & color fragment shader layout
struct PipelineLayout {
  VkPipelineLayout layout{VK_NULL_HANDLE};

  PipelineLayout() = default;
  ~PipelineLayout();
  void Create(VkDevice device);
  PipelineLayout(const PipelineLayout &) = delete;
  PipelineLayout &operator=(const PipelineLayout &) = delete;
  PipelineLayout(PipelineLayout &&) = delete;
  PipelineLayout &operator=(PipelineLayout &&) = delete;

private:
  VkDevice m_vkDevice{VK_NULL_HANDLE};
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
  Create(VkDevice device, VkExtent2D size, const PipelineLayout &layout,
         const class RenderPass &rp, const struct ShaderProgram &sp,
         const std::shared_ptr<struct VertexBuffer> &vb);

  void Dynamic(VkDynamicState state);
  void Release();
};

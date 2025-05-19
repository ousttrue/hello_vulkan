#pragma once
#include <array>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

std::string vkResultString(VkResult res);

[[noreturn]] void ThrowVkResult(VkResult res, const char *originator = nullptr,
                                const char *sourceLocation = nullptr);

VkResult CheckVkResult(VkResult res, const char *originator = nullptr,
                       const char *sourceLocation = nullptr);

// CmdBuffer - manage VkCommandBuffer state
struct CmdBuffer {
#define LIST_CMDBUFFER_STATES(_)                                               \
  _(Undefined)                                                                 \
  _(Initialized)                                                               \
  _(Recording)                                                                 \
  _(Executable)                                                                \
  _(Executing)
  enum class CmdBufferState {
#define MK_ENUM(name) name,
    LIST_CMDBUFFER_STATES(MK_ENUM)
#undef MK_ENUM
  };
  CmdBufferState state{CmdBufferState::Undefined};
  VkCommandPool pool{VK_NULL_HANDLE};
  VkCommandBuffer buf{VK_NULL_HANDLE};
  VkFence execFence{VK_NULL_HANDLE};

  CmdBuffer() = default;
  CmdBuffer(const CmdBuffer &) = delete;
  CmdBuffer &operator=(const CmdBuffer &) = delete;
  CmdBuffer(CmdBuffer &&) = delete;
  CmdBuffer &operator=(CmdBuffer &&) = delete;
  ~CmdBuffer();
  std::string StateString(CmdBufferState s);
  bool Init(const class VulkanDebugObjectNamer &namer, VkDevice device,
            uint32_t queueFamilyIndex);
  bool Begin();
  bool End();
  bool Exec(VkQueue queue);
  bool Wait();
  bool Reset();

private:
  VkDevice m_vkDevice{VK_NULL_HANDLE};
  void SetState(CmdBufferState newState) { state = newState; }
  // #undef LIST_CMDBUFFER_STATES
};

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

// VkImage + framebuffer wrapper
struct RenderTarget {
  VkImage colorImage{VK_NULL_HANDLE};
  VkImage depthImage{VK_NULL_HANDLE};
  VkImageView colorView{VK_NULL_HANDLE};
  VkImageView depthView{VK_NULL_HANDLE};
  VkFramebuffer fb{VK_NULL_HANDLE};

  RenderTarget() = default;
  ~RenderTarget();

  RenderTarget(RenderTarget &&other) noexcept;
  RenderTarget &operator=(RenderTarget &&other) noexcept;
  void Create(const VulkanDebugObjectNamer &namer, VkDevice device,
              VkImage aColorImage, VkImage aDepthImage, VkExtent2D size,
              struct RenderPass &renderPass);

  RenderTarget(const RenderTarget &) = delete;
  RenderTarget &operator=(const RenderTarget &) = delete;

private:
  VkDevice m_vkDevice{VK_NULL_HANDLE};
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

#include "VertexBuffer.h"

// Pipeline wrapper for rendering pipeline state
struct Pipeline {
  VkPipeline pipe{VK_NULL_HANDLE};
  VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  std::vector<VkDynamicState> dynamicStateEnables;

  Pipeline() = default;

  void Dynamic(VkDynamicState state);
  void Create(VkDevice device, VkExtent2D size, const PipelineLayout &layout,
              const RenderPass &rp, const ShaderProgram &sp,
              const VertexBufferBase &vb);
  void Release();

private:
  VkDevice m_vkDevice{VK_NULL_HANDLE};
};

struct DepthBuffer {
  VkDeviceMemory depthMemory{VK_NULL_HANDLE};
  VkImage depthImage{VK_NULL_HANDLE};

  DepthBuffer() = default;
  ~DepthBuffer();

  DepthBuffer(DepthBuffer &&other) noexcept;
  DepthBuffer &operator=(DepthBuffer &&other) noexcept;
  void Create(const VulkanDebugObjectNamer &namer, VkDevice device,
              MemoryAllocator *memAllocator, VkFormat depthFormat,
              const struct XrSwapchainCreateInfo &swapchainCreateInfo);

  void TransitionLayout(CmdBuffer *cmdBuffer, VkImageLayout newLayout);
  DepthBuffer(const DepthBuffer &) = delete;
  DepthBuffer &operator=(const DepthBuffer &) = delete;

private:
  VkDevice m_vkDevice{VK_NULL_HANDLE};
  VkImageLayout m_vkLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

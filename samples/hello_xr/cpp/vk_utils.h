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
              class RenderPass &renderPass);

  RenderTarget(const RenderTarget &) = delete;
  RenderTarget &operator=(const RenderTarget &) = delete;

private:
  VkDevice m_vkDevice{VK_NULL_HANDLE};
};

#include "VertexBuffer.h"

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

#pragma once
#include <string>
#include <vulkan/vulkan.h>

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
  bool Init(VkDevice device, uint32_t queueFamilyIndex);
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

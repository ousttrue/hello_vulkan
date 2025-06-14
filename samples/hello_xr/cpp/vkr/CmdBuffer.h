#pragma once
#include <memory>
#include <string>
#include <vulkan/vulkan.h>

// CmdBuffer - manage VkCommandBuffer state
class CmdBuffer {
  VkDevice m_vkDevice{VK_NULL_HANDLE};
  VkQueue m_queue;

  // enum class CmdBufferState {
  //   Undefined,
  //   Initialized,
  //   Recording,
  //   Executable,
  //   Executing,
  // };
  // CmdBufferState state{CmdBufferState::Undefined};
  // void SetState(CmdBufferState newState) { state = newState; }

  VkCommandPool pool{VK_NULL_HANDLE};
  VkFence execFence{VK_NULL_HANDLE};

  CmdBuffer() = default;
  CmdBuffer(const CmdBuffer &) = delete;
  CmdBuffer &operator=(const CmdBuffer &) = delete;
  CmdBuffer(CmdBuffer &&) = delete;
  CmdBuffer &operator=(CmdBuffer &&) = delete;

public:
  VkCommandBuffer buf{VK_NULL_HANDLE};
  ~CmdBuffer();
  // std::string StateString(CmdBufferState s) {
  //   switch (s) {
  //   case CmdBufferState::Undefined:
  //     return "Undefined";
  //   case CmdBufferState::Initialized:
  //     return "Initialized";
  //   case CmdBufferState::Recording:
  //     return "Recording";
  //   case CmdBufferState::Executable:
  //     return "Executable";
  //   case CmdBufferState::Executing:
  //     return "Executing";
  //   }
  //   return "(Unknown)";
  // }

  static std::shared_ptr<CmdBuffer> Create(VkDevice device,
                                           uint32_t queueFamilyIndex);

  bool Begin();
  bool End();
  bool Exec();
  bool Wait();
  bool Reset();
};

#pragma once
#include <memory>
#include <vulkan/vulkan.h>

class CmdBuffer {
  VkDevice m_vkDevice{VK_NULL_HANDLE};
  VkQueue m_queue;

  VkCommandPool pool{VK_NULL_HANDLE};

  CmdBuffer() = default;
  CmdBuffer(const CmdBuffer &) = delete;
  CmdBuffer &operator=(const CmdBuffer &) = delete;
  CmdBuffer(CmdBuffer &&) = delete;
  CmdBuffer &operator=(CmdBuffer &&) = delete;

public:
  VkCommandBuffer buf{VK_NULL_HANDLE};
  ~CmdBuffer();
  static std::shared_ptr<CmdBuffer> Create(VkDevice device,
                                           uint32_t queueFamilyIndex);
  bool Begin();
  bool End();
  bool Exec(VkFence execFence);
  bool Reset();
};

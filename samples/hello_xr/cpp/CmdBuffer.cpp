#include "CmdBuffer.h"
#include "logger.h"
#include "vulkan_debug_object_namer.hpp"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

//
// CmdBuffer - manage VkCommandBuffer state
//
CmdBuffer::~CmdBuffer() {
  SetState(CmdBufferState::Undefined);
  if (m_vkDevice != nullptr) {
    if (buf != VK_NULL_HANDLE) {
      vkFreeCommandBuffers(m_vkDevice, pool, 1, &buf);
    }
    if (pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(m_vkDevice, pool, nullptr);
    }
    if (execFence != VK_NULL_HANDLE) {
      vkDestroyFence(m_vkDevice, execFence, nullptr);
    }
  }
  buf = VK_NULL_HANDLE;
  pool = VK_NULL_HANDLE;
  execFence = VK_NULL_HANDLE;
  m_vkDevice = nullptr;
}

std::string CmdBuffer::StateString(CmdBufferState s) {
  switch (s) {
#define MK_CASE(name)                                                          \
  case CmdBufferState::name:                                                   \
    return #name;
    LIST_CMDBUFFER_STATES(MK_CASE)
#undef MK_CASE
  }
  return "(Unknown)";
}

#define CHECK_CBSTATE(s)                                                       \
  do                                                                           \
    if (state != (s)) {                                                        \
      Log::Write(Log::Level::Error,                                            \
                 std::string("Expecting state " #s " from ") + __FUNCTION__ +  \
                     ", in " + StateString(state));                            \
      return false;                                                            \
    }                                                                          \
  while (0)

bool CmdBuffer::Init(VkDevice device, uint32_t queueFamilyIndex) {
  CHECK_CBSTATE(CmdBufferState::Undefined);

  m_vkDevice = device;

  // Create a command pool to allocate our command buffer from
  VkCommandPoolCreateInfo cmdPoolInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queueFamilyIndex,
  };
  if (vkCreateCommandPool(m_vkDevice, &cmdPoolInfo, nullptr, &pool) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateCommandPool");
  }
  if (SetDebugUtilsObjectNameEXT(device, VK_OBJECT_TYPE_COMMAND_POOL,
                                 (uint64_t)pool,
                                 "hello_xr command pool") != VK_SUCCESS) {
    throw std::runtime_error("SetDebugUtilsObjectNameEXT");
  }

  // Create the command buffer from the command pool
  VkCommandBufferAllocateInfo cmd{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
  };
  cmd.commandPool = pool;
  cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(m_vkDevice, &cmd, &buf) != VK_SUCCESS) {
    throw std::runtime_error("vkAllocateCommandBuffers");
  }
  if (SetDebugUtilsObjectNameEXT(device, VK_OBJECT_TYPE_COMMAND_BUFFER,
                                 (uint64_t)buf,
                                 "hello_xr command buffer") != VK_SUCCESS) {
    throw std::runtime_error("SetDebugUtilsObjectNameEXT");
  }

  VkFenceCreateInfo fenceInfo{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
  };
  if (vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &execFence) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateFence");
  }
  if (SetDebugUtilsObjectNameEXT(device, VK_OBJECT_TYPE_FENCE,
                                 (uint64_t)execFence,
                                 "hello_xr fence") != VK_SUCCESS) {
    throw std::runtime_error("SetDebugUtilsObjectNameEXT");
  }

  SetState(CmdBufferState::Initialized);
  return true;
}

bool CmdBuffer::Begin() {
  CHECK_CBSTATE(CmdBufferState::Initialized);
  VkCommandBufferBeginInfo cmdBeginInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  if (vkBeginCommandBuffer(buf, &cmdBeginInfo) != VK_SUCCESS) {
    throw std::runtime_error("vkBeginCommandBuffer");
  }
  SetState(CmdBufferState::Recording);
  return true;
}

bool CmdBuffer::End() {
  CHECK_CBSTATE(CmdBufferState::Recording);
  if (vkEndCommandBuffer(buf) != VK_SUCCESS) {
    throw std::runtime_error("vkEndCommandBuffer");
  }
  SetState(CmdBufferState::Executable);
  return true;
}

bool CmdBuffer::Exec(VkQueue queue) {
  CHECK_CBSTATE(CmdBufferState::Executable);

  VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &buf;
  if (vkQueueSubmit(queue, 1, &submitInfo, execFence) != VK_SUCCESS) {
    throw std::runtime_error("vkQueueSubmit");
  }

  SetState(CmdBufferState::Executing);
  return true;
}

bool CmdBuffer::Wait() {
  // Waiting on a not-in-flight command buffer is a no-op
  if (state == CmdBufferState::Initialized) {
    return true;
  }

  CHECK_CBSTATE(CmdBufferState::Executing);

  const uint32_t timeoutNs = 1 * 1000 * 1000 * 1000;
  for (int i = 0; i < 5; ++i) {
    auto res = vkWaitForFences(m_vkDevice, 1, &execFence, VK_TRUE, timeoutNs);
    if (res == VK_SUCCESS) {
      // Buffer can be executed multiple times...
      SetState(CmdBufferState::Executable);
      return true;
    }
    Log::Write(Log::Level::Info,
               "Waiting for CmdBuffer fence timed out, retrying...");
  }

  return false;
}

bool CmdBuffer::Reset() {
  if (state != CmdBufferState::Initialized) {
    CHECK_CBSTATE(CmdBufferState::Executable);

    if (vkResetFences(m_vkDevice, 1, &execFence) != VK_SUCCESS) {
      throw std::runtime_error("vkResetFences");
    }
    if (vkResetCommandBuffer(buf, 0) != VK_SUCCESS) {
      throw std::runtime_error("vkResetCommandBuffer");
    }

    SetState(CmdBufferState::Initialized);
  }

  return true;
}

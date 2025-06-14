#include "CmdBuffer.h"
#include <common/logger.h>
#include <stdexcept>
#include <vko/vko.h>
#include <vulkan/vulkan_core.h>

//
// CmdBuffer - manage VkCommandBuffer state
//
CmdBuffer::~CmdBuffer() {
  // SetState(CmdBufferState::Undefined);
  if (m_vkDevice != nullptr) {
    if (buf != VK_NULL_HANDLE) {
      vkFreeCommandBuffers(m_vkDevice, pool, 1, &buf);
    }
    if (pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(m_vkDevice, pool, nullptr);
    }
  }
  buf = VK_NULL_HANDLE;
  pool = VK_NULL_HANDLE;
  m_vkDevice = nullptr;
}

std::shared_ptr<CmdBuffer> CmdBuffer::Create(VkDevice device,
                                             uint32_t queueFamilyIndex) {

  auto ptr = std::shared_ptr<CmdBuffer>(new CmdBuffer());

  ptr->m_vkDevice = device;
  vkGetDeviceQueue(device, queueFamilyIndex, 0, &ptr->m_queue);

  // Create a command pool to allocate our command buffer from
  VkCommandPoolCreateInfo cmdPoolInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queueFamilyIndex,
  };
  if (vkCreateCommandPool(ptr->m_vkDevice, &cmdPoolInfo, nullptr, &ptr->pool) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateCommandPool");
  }
  if (SetDebugUtilsObjectNameEXT(device, VK_OBJECT_TYPE_COMMAND_POOL,
                                 (uint64_t)ptr->pool,
                                 "hello_xr command pool") != VK_SUCCESS) {
    throw std::runtime_error("SetDebugUtilsObjectNameEXT");
  }

  // Create the command buffer from the command pool
  VkCommandBufferAllocateInfo cmd{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
  };
  cmd.commandPool = ptr->pool;
  cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(ptr->m_vkDevice, &cmd, &ptr->buf) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkAllocateCommandBuffers");
  }
  if (SetDebugUtilsObjectNameEXT(device, VK_OBJECT_TYPE_COMMAND_BUFFER,
                                 (uint64_t)ptr->buf,
                                 "hello_xr command buffer") != VK_SUCCESS) {
    throw std::runtime_error("SetDebugUtilsObjectNameEXT");
  }

  // ptr->SetState(CmdBufferState::Initialized);
  return ptr;
}

bool CmdBuffer::Begin() {
  // CHECK_CBSTATE(CmdBufferState::Initialized);
  VkCommandBufferBeginInfo cmdBeginInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  if (vkBeginCommandBuffer(buf, &cmdBeginInfo) != VK_SUCCESS) {
    throw std::runtime_error("vkBeginCommandBuffer");
  }
  // SetState(CmdBufferState::Recording);
  return true;
}

bool CmdBuffer::End() {
  // CHECK_CBSTATE(CmdBufferState::Recording);
  if (vkEndCommandBuffer(buf) != VK_SUCCESS) {
    throw std::runtime_error("vkEndCommandBuffer");
  }
  // SetState(CmdBufferState::Executable);
  return true;
}

bool CmdBuffer::Exec(VkFence execFence) {
  // CHECK_CBSTATE(CmdBufferState::Executable);

  VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &buf;
  if (vkQueueSubmit(m_queue, 1, &submitInfo, execFence) != VK_SUCCESS) {
    throw std::runtime_error("vkQueueSubmit");
  }

  // SetState(CmdBufferState::Executing);
  return true;
}

bool CmdBuffer::Reset() {
  // if (state != CmdBufferState::Initialized) {
  // CHECK_CBSTATE(CmdBufferState::Executable);

  if (vkResetCommandBuffer(buf, 0) != VK_SUCCESS) {
    throw std::runtime_error("vkResetCommandBuffer");
  }

  // SetState(CmdBufferState::Initialized);
  // }

  return true;
}

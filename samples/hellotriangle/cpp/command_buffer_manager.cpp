#include "command_buffer_manager.hpp"
#include "common.hpp"

CommandBufferManager::CommandBufferManager(VkDevice vkDevice,
                                           VkCommandBufferLevel bufferLevel,
                                           unsigned graphicsQueueIndex)
    : _device(vkDevice), _commandBufferLevel(bufferLevel), _count(0) {
  VkCommandPoolCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = graphicsQueueIndex,
  };
  VK_CHECK(vkCreateCommandPool(_device, &info, nullptr, &_pool));
}

CommandBufferManager::~CommandBufferManager() {
  if (!_buffers.empty()) {
    vkFreeCommandBuffers(_device, _pool, _buffers.size(), _buffers.data());
  }
  vkDestroyCommandPool(_device, _pool, nullptr);
}

void CommandBufferManager::beginFrame() {
  _count = 0;
  vkResetCommandPool(_device, _pool, 0);
}

VkCommandBuffer CommandBufferManager::requestCommandBuffer() {
  if (_count >= _buffers.size()) {
    VkCommandBufferAllocateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = _pool,
        .level = _commandBufferLevel,
        .commandBufferCount = 1,
    };
    VkCommandBuffer ret = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(_device, &info, &ret));
    _buffers.push_back(ret);
  }
  return _buffers[_count++];
}

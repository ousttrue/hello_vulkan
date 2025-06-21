#pragma once
#include "../vk.h"
#include "vuloxr.h"
#include <span>

namespace vuloxr {

namespace vk {

inline VkBuffer createVertexBuffer(VkDevice device, uint32_t bufferSize) {
  VkBufferCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = bufferSize,
      .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
  };
  VkBuffer buffer;
  vuloxr::vk::CheckVkResult(vkCreateBuffer(device, &info, nullptr, &buffer));
  return buffer;
}

struct Mesh : NonCopyable {
  VkDevice device;
  VkBuffer vertices;
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;

  ~Mesh() { vkDestroyBuffer(this->device, this->vertices, nullptr); }

  static Mesh
  create(VkDevice device, uint32_t bufferSize,
         const std::vector<VkVertexInputBindingDescription> &bindings,
         const std::vector<VkVertexInputAttributeDescription> &attributes) {
    auto buffer = createVertexBuffer(device, bufferSize);
    return {
        .device = device,
        .vertices = buffer,
        .bindings = bindings,
        .attributes = attributes,
    };
  }

  void draw(VkCommandBuffer cmd, VkPipeline pipeline, uint32_t vertexCount) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &this->vertices, &offset);
    vkCmdDraw(cmd, vertexCount, 1, 0, 0);
  }
};

} // namespace vk
} // namespace vuloxr

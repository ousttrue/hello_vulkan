#include "DepthBuffer.h"
#include "CmdBuffer.h"
#include "MemoryAllocator.h"
#include "vulkan_debug_object_namer.hpp"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

DepthBuffer::~DepthBuffer() {
  if (m_vkDevice != nullptr) {
    if (depthImage != VK_NULL_HANDLE) {
      vkDestroyImage(m_vkDevice, depthImage, nullptr);
    }
    if (depthMemory != VK_NULL_HANDLE) {
      vkFreeMemory(m_vkDevice, depthMemory, nullptr);
    }
  }
  depthImage = VK_NULL_HANDLE;
  depthMemory = VK_NULL_HANDLE;
  m_vkDevice = nullptr;
}

// DepthBuffer::DepthBuffer(DepthBuffer &&other) noexcept {
//   using std::swap;
//
//   swap(depthImage, other.depthImage);
//   swap(depthMemory, other.depthMemory);
//   swap(m_vkDevice, other.m_vkDevice);
// }
//
// DepthBuffer &DepthBuffer::operator=(DepthBuffer &&other) noexcept {
//   if (&other == this) {
//     return *this;
//   }
//   // clean up self
//   this->~DepthBuffer();
//   using std::swap;
//
//   swap(depthImage, other.depthImage);
//   swap(depthMemory, other.depthMemory);
//   swap(m_vkDevice, other.m_vkDevice);
//   return *this;
// }

std::shared_ptr<DepthBuffer> DepthBuffer::Create(
    VkDevice device, const std::shared_ptr<MemoryAllocator> &memAllocator,
    VkExtent2D size, VkFormat depthFormat, VkSampleCountFlagBits sampleCount) {
  auto ptr = std::shared_ptr<DepthBuffer>(new DepthBuffer());
  ptr->m_vkDevice = device;

  // Create a D32 depthbuffer
  VkImageCreateInfo imageInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = depthFormat,
      .extent =
          {
              .width = size.width,
              .height = size.height,
              .depth = 1,
          },
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = sampleCount,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  if (vkCreateImage(device, &imageInfo, nullptr, &ptr->depthImage) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateImage");
  }
  if (SetDebugUtilsObjectNameEXT(
          device, VK_OBJECT_TYPE_IMAGE, (uint64_t)ptr->depthImage,
          "hello_xr fallback depth image") != VK_SUCCESS) {
    throw std::runtime_error("SetDebugUtilsObjectNameEXT");
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device, ptr->depthImage, &memRequirements);
  memAllocator->Allocate(memRequirements, &ptr->depthMemory,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (SetDebugUtilsObjectNameEXT(
          device, VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)ptr->depthMemory,
          "hello_xr fallback depth image memory") != VK_SUCCESS) {
    throw std::runtime_error("SetDebugUtilsObjectNameEXT");
  }
  if (vkBindImageMemory(device, ptr->depthImage, ptr->depthMemory, 0) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkBindImageMemory");
  }
  return ptr;
}

void DepthBuffer::TransitionLayout(VkCommandBuffer cmd,
                                   VkImageLayout newLayout) {
  if (newLayout != m_vkLayout) {
    VkImageMemoryBarrier depthBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        .oldLayout = m_vkLayout,
        .newLayout = newLayout,
        .image = depthImage,
        .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                         VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &depthBarrier);
    m_vkLayout = newLayout;
  }
}

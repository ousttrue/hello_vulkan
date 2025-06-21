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
  CheckVkResult(vkCreateBuffer(device, &info, nullptr, &buffer));
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

struct Texture : NonCopyable {
  VkDevice device;
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView imageView = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

  VkDescriptorImageInfo descriptorInfo{
      // .sampler = texture.sampler,
      // .imageView = texture.imageView,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  Texture(VkDevice _device, uint32_t width, uint32_t height) : device(_device) {

    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = VK_SHARING_MODE_EXCLUSIVE,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = {.width = width, .height = height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    CheckVkResult(
        vkCreateImage(this->device, &imageInfo, nullptr, &this->image));

  }

  void setMemory(VkDeviceMemory memory)
  {
    this->memory = memory;

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = this->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB, // format;
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1},
    };
    CheckVkResult(
        vkCreateImageView(device, &viewInfo, nullptr, &this->imageView));

    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        // next two lines describe how to interpolate texels that are
        // magnified or minified
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        // note: image axes are UVW (rather than XYZ)
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        // repeat the texture when out of bounds
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = 16.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    CheckVkResult(
        vkCreateSampler(device, &samplerInfo, nullptr, &this->sampler));

    this->descriptorInfo.sampler = this->sampler;
    this->descriptorInfo.imageView = this->imageView;
  }

  ~Texture() {
    vkDestroySampler(this->device, this->sampler, nullptr);
    vkDestroyImageView(this->device, this->imageView, nullptr);
    vkFreeMemory(device, this->memory, nullptr);
    vkDestroyImage(this->device, this->image, nullptr);
  }
};

} // namespace vk
} // namespace vuloxr

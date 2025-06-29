#pragma once
#include "../vk.h"
#include "vuloxr.h"

namespace vuloxr {

namespace vk {

struct Buffer : NonCopyable {
  VkDevice device;
  VkBuffer buffer;
  operator VkBuffer() const { return this->buffer; }
  Buffer(VkDevice _device, uint32_t bufferSize, VkBufferUsageFlags usage)
      : device(_device) {
    VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bufferSize,
        .usage = usage,
    };
    CheckVkResult(vkCreateBuffer(this->device, &info, nullptr, &this->buffer));
  }
  ~Buffer() { vkDestroyBuffer(this->device, this->buffer, nullptr); }
};

template <typename T> struct UniformBuffer : NonCopyable {
  VkDevice device;
  Buffer buffer;
  Memory memory;
  T value;
  UniformBuffer(VkDevice device)
      : buffer(device, sizeof(T), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
        memory(device) {}
  void mapWrite() const { this->memory.mapWrite(&this->value, sizeof(T)); }
};

struct VertexBuffer : NonCopyable {
  VkDevice device;
  Buffer buffer;
  Memory memory;
  uint32_t drawCount = 0;
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;

  static VertexBuffer
  create(VkDevice device, uint32_t bufferSize, uint32_t drawCount,
         const std::vector<VkVertexInputBindingDescription> &bindings,
         const std::vector<VkVertexInputAttributeDescription> &attributes) {
    return {
        .device = device,
        .buffer = Buffer(device, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
        .memory = Memory(device),
        .drawCount = drawCount,
        .bindings = bindings,
        .attributes = attributes,
    };
  }

  void draw(VkCommandBuffer cmd, VkPipeline pipeline) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    VkDeviceSize offset = 0;
    VkBuffer buffer = this->buffer;
    vkCmdBindVertexBuffers(cmd, 0, 1, &buffer, &offset);
    vkCmdDraw(cmd, this->drawCount, 1, 0, 0);
  }
};

struct IndexBuffer : NonCopyable {
  VkDevice device = VK_NULL_HANDLE;
  Buffer buffer;
  Memory memory;
  uint32_t drawCount;
  VkIndexType indexType = VK_INDEX_TYPE_UINT16;

  static IndexBuffer create(VkDevice device, uint32_t bufferSize,
                            uint32_t drawCount, VkIndexType indexType) {
    return {
        .device = device,
        .buffer = Buffer(device, bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
        .memory = Memory(device),
        .drawCount = drawCount,
        .indexType = indexType,
    };
  }

  void draw(VkCommandBuffer cmd, VkPipeline pipeline, VkBuffer vertices) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertices, &offset);
    vkCmdBindIndexBuffer(cmd, this->buffer, 0, this->indexType);
    vkCmdDrawIndexed(cmd, this->drawCount, 1, 0, 0, 0);
  }
};

struct DepthImage : NonCopyable {
  VkDevice device;
  VkImage image = VK_NULL_HANDLE;
  Memory memory;
  // VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

  // VkDeviceMemory depthMemory{VK_NULL_HANDLE};
  // VkDevice m_vkDevice{VK_NULL_HANDLE};
  // VkImage depthImage{VK_NULL_HANDLE};

  DepthImage(VkDevice _device, VkExtent2D size, VkFormat depthFormat,
             VkSampleCountFlagBits sampleCount)
      // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
      : device(_device), memory(_device) {

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
    CheckVkResult(
        vkCreateImage(this->device, &imageInfo, nullptr, &this->image));
    // if (SetDebugUtilsObjectNameEXT(
    //         device, VK_OBJECT_TYPE_IMAGE, (uint64_t)ptr->depthImage,
    //         "hello_xr fallback depth image") != VK_SUCCESS) {
    //   throw std::runtime_error("SetDebugUtilsObjectNameEXT");
    // }

    // VkMemoryRequirements memRequirements;
    // vkGetImageMemoryRequirements(_device, image, &memRequirements);
    // this->memory = std::make_shared<DeviceMemory>(this->device,
    // physicalDevice,
    //                                               memRequirements,
    //                                               properties);

    // auto memAllocator = MemoryAllocator::Create(physicalDevice, device);
    // ptr->depthMemory = memAllocator->Allocate( ptr->depthImage,
    // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); if (SetDebugUtilsObjectNameEXT(
    //         device, VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)ptr->depthMemory,
    //         "hello_xr fallback depth image memory") != VK_SUCCESS) {
    //   throw std::runtime_error("SetDebugUtilsObjectNameEXT");
    // }
    // VKO_CHECK(vkBindImageMemory(this->device, this->image, *this->memory,
    // 0));
  }
  ~DepthImage() { vkDestroyImage(this->device, this->image, nullptr); }
};

struct Texture : NonCopyable {
  VkDevice device;
  VkImage image = VK_NULL_HANDLE;
  Memory memory;
  VkImageView imageView = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

  VkDescriptorImageInfo descriptorInfo{
      // .sampler = texture.sampler,
      // .imageView = texture.imageView,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  Texture(VkDevice _device, uint32_t width, uint32_t height)
      : device(_device), memory(_device) {

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

  void setMemory(Memory memory) {
    this->memory = std::move(memory);

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
    vkDestroyImage(this->device, this->image, nullptr);
  }
};

} // namespace vk
} // namespace vuloxr

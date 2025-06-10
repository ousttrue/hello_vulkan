#include "scene.h"
#include "vko/vko.h"

//
// Texture
//
static void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                                  VkFormat format, VkImageLayout oldLayout,
                                  VkImageLayout newLayout) {

  VkImageMemoryBarrier barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      // can also use VK_IMAGE_LAYOUT_UNDEFINED if we don't care about the
      // existing contents of image!
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;
  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
}

static void copyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer,
                              VkImage image, uint32_t width, uint32_t height) {
  VkBufferImageCopy region{
      .bufferOffset = 0,
      // functions as "padding" for the image destination
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .mipLevel = 0,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
      .imageOffset = {0, 0, 0},
      .imageExtent = {width, height, 1},
  };
  vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

Texture::Texture(VkPhysicalDevice physicalDevice, VkDevice _device,
                 uint32_t graphicsQueueFamilyIndex, uint32_t width,
                 uint32_t height, const uint8_t *pixels)
    : device(_device),
      image(physicalDevice, _device, width, height, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {

  int texChannels = 4;
  VkDeviceSize imageSize = width * height * 4;

  auto stagingBuffer = std::make_shared<vko::Buffer>(
      physicalDevice, device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  stagingBuffer->memory->assign(pixels, imageSize);

  vko::CommandPool commandPool(device, graphicsQueueFamilyIndex);

  vko::executeCommandSync(
      device, commandPool.queue, commandPool,
      [self = this](auto commandBuffer) {
        transitionImageLayout(commandBuffer, self->image.image,
                              VK_FORMAT_R8G8B8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      });

  vko::executeCommandSync(
      device, commandPool.queue, commandPool,

      [self = this, stagingBuffer, width, height](auto commandBuffer) {
        copyBufferToImage(commandBuffer, stagingBuffer->buffer,
                          self->image.image, static_cast<uint32_t>(width),
                          static_cast<uint32_t>(height));
      });

  vko::executeCommandSync(device, commandPool.queue, commandPool,
                          [self = this](auto commandBuffer) {
                            transitionImageLayout(
                                commandBuffer, self->image.image,
                                VK_FORMAT_R8G8B8A8_SRGB,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                          });

  {
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = this->image.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB, // format;
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1},
    };
    VKO_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &this->imageView));
  }
  {
    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        // next two lines describe how to interpolate texels that are magnified
        // or minified
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
    VKO_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &this->sampler));
  }
}

Texture::~Texture() {
  vkDestroySampler(this->device, this->sampler, nullptr);
  vkDestroyImageView(this->device, this->imageView, nullptr);
}

//
// Scene
//
Scene::Scene(VkPhysicalDevice physicalDevice, VkDevice _device,
             uint32_t graphicsQueueFamilyIndex)
    : device(_device) {

  uint8_t pixels[] = {
      255, 0,   0,   255, // R
      0,   255, 0,   255, // G
      0,   0,   255, 255, // B
      255, 255, 255, 255, // WHITE
  };
  this->texture = std::make_shared<Texture>(
      physicalDevice, _device, graphicsQueueFamilyIndex, 2, 2, pixels);

  {
    // Interleaving vertex attributes - includes position AND color attributes!
    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // top-left and RED
        {{0.5f, -0.5f},
         {0.0f, 1.0f, 0.0f},
         {1.0f, 0.0f}}, // top-right and GREEN
        {{0.5f, 0.5f},
         {0.0f, 0.0f, 1.0f},
         {1.0f, 1.0f}}, // bottom-right and BLUE
        {{-0.5f, 0.5f},
         {1.0f, 1.0f, 1.0f},
         {0.0f, 1.0f}} // bottom-left and WHITE
    };

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    auto stagingBuffer = std::make_shared<vko::Buffer>(
        physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    stagingBuffer->memory->assign(vertices.data(), bufferSize);
    this->vertexBuffer = std::make_shared<vko::Buffer>(
        physicalDevice, device, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vko::CommandPool commandPool(device, graphicsQueueFamilyIndex);
    vko::executeCommandSync(device, commandPool.queue, commandPool,
                            [stagingBuffer, vertexBuffer = this->vertexBuffer,
                             bufferSize](auto commandBuffer) {
                              stagingBuffer->copyCommand(commandBuffer,
                                                         vertexBuffer->buffer,
                                                         bufferSize);
                            });
  }

  {
    const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};
    this->indexDrawCount = indices.size();
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    auto stagingBuffer = std::make_shared<vko::Buffer>(
        physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    stagingBuffer->memory->assign(indices.data(), bufferSize);

    this->indexBuffer = std::make_shared<vko::Buffer>(
        physicalDevice, device, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vko::CommandPool commandPool(device, graphicsQueueFamilyIndex);
    vko::executeCommandSync(device, commandPool.queue, commandPool,
                            [stagingBuffer, indexBuffer = this->indexBuffer,
                             bufferSize](auto commandBuffer) {
                              stagingBuffer->copyCommand(commandBuffer,
                                                         indexBuffer->buffer,
                                                         bufferSize);
                            });
  }
}

Scene::~Scene() {}

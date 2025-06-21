#include "../main_loop.h"
#include "../glsl_to_spv.h"
#include <vko/vko_pipeline.h>
#include <vuloxr/vk.h>
#include <vuloxr/vk/buffer.h>
#include <vuloxr/vk/pipeline.h>

const char VS[] = {
#embed "texture.vert"
    , 0, 0, 0, 0};

#include <glm/fwd.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/ext/matrix_projection.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Matrix {
  float m[16];
};

struct UniformBufferObject {
  Matrix model;
  Matrix view;
  Matrix proj;
  void setTime(float time, float width, float height) {
    // time * radians(90.0f) = rotation of 90 degrees per second!
    *((glm::mat4 *)&this->model) =
        glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
    // specify view position
    *((glm::mat4 *)&this->view) =
        glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
    // view from a 45 degree angle
    *((glm::mat4 *)&this->proj) =
        glm::perspective(glm::radians(45.0f), width / height, 1.0f, 10.0f);

    // GLM was designed for OpenGL, therefor Y-coordinate (of the clip
    // coodinates) is inverted, the following code flips this around!
    this->proj.m[5] /*[1][1]*/ *= -1;
  }
};

const char FS[] = {
#embed "texture.frag"
    , 0, 0, 0, 0};

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

struct Vec2 {
  float x;
  float y;
};

struct Vec3 {
  float x;
  float y;
  float z;
};

struct Vertex {
  Vec2 pos;
  Vec3 color;
  Vec2 texCoord;
};

void main_loop(const std::function<bool()> &runLoop,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *) {

  vuloxr::vk::Texture texture(device, 2, 2);
  auto textureMemory =
      physicalDevice.allocMemoryForImage(device, texture.image);
  texture.setMemory(textureMemory);

  {
    // upload image
    uint8_t pixels[] = {
        255, 0,   0,   255, // R
        0,   255, 0,   255, // G
        0,   0,   255, 255, // B
        255, 255, 255, 255, // WHITE
    };
    VkDeviceSize imageSize = 2 * 2 * 4;

    auto stagingBuffer = std::make_shared<vko::Buffer>(
        physicalDevice, device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    stagingBuffer->memory->assign(pixels, imageSize);

    vko::CommandPool commandPool(device, physicalDevice.graphicsFamilyIndex);

    vko::executeCommandSync(device, commandPool.queue, commandPool,
                            [image = texture.image](auto commandBuffer) {
                              transitionImageLayout(
                                  commandBuffer, image, VK_FORMAT_R8G8B8_SRGB,
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                            });

    vko::executeCommandSync(device, commandPool.queue, commandPool,

                            [image = texture.image, stagingBuffer, width = 2,
                             height = 2](auto commandBuffer) {
                              copyBufferToImage(commandBuffer,
                                                stagingBuffer->buffer, image,
                                                static_cast<uint32_t>(width),
                                                static_cast<uint32_t>(height));
                            });

    vko::executeCommandSync(device, commandPool.queue, commandPool,
                            [image = texture.image](auto commandBuffer) {
                              transitionImageLayout(
                                  commandBuffer, image, VK_FORMAT_R8G8B8A8_SRGB,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                            });
  }

  vko::IndexedMesh mesh = {
      .inputBindingDescriptions =
          {
              {
                  .binding = 0,
                  .stride = static_cast<uint32_t>(sizeof(Vertex)),
                  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
              },
          },
      .attributeDescriptions = {
          // describes position
          {
              .location = 0,
              .binding = 0,
              .format = VK_FORMAT_R32G32_SFLOAT,
              .offset = offsetof(Vertex, pos),
          },
          // describes color
          {
              .location = 1,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32_SFLOAT,
              .offset = offsetof(Vertex, color),
          },
          // uv
          {
              .location = 2,
              .binding = 0,
              .format = VK_FORMAT_R32G32_SFLOAT,
              .offset = offsetof(Vertex, texCoord),
          },
      }};
  {
    // Interleaving vertex attributes - includes position AND color
    // attributes!
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
    mesh.vertexBuffer = std::make_shared<vko::Buffer>(
        physicalDevice, device, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vko::copyBytesToBufferCommand(
        physicalDevice, device, physicalDevice.graphicsFamilyIndex,
        vertices.data(), bufferSize, mesh.vertexBuffer->buffer);
  }

  {
    const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};
    mesh.indexDrawCount = indices.size();
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
    mesh.indexBuffer = std::make_shared<vko::Buffer>(
        physicalDevice, device, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vko::copyBytesToBufferCommand(
        physicalDevice, device, physicalDevice.graphicsFamilyIndex,
        indices.data(), bufferSize, mesh.indexBuffer->buffer);
  }

  vko::DescriptorSets descriptorSets(
      device,
      {
          {
              .binding = 0,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
              .pImmutableSamplers = nullptr,
          },
          {
              .binding = 1,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              // indicate that we want to use the combined image sampler
              // descriptor in the fragment shader
              .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
              .pImmutableSamplers = nullptr,
          },
      });
  descriptorSets.allocate(
      swapchain.images.size(),
      {
          VkDescriptorPoolSize{
              .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = static_cast<uint32_t>(swapchain.images.size()),
          },
          VkDescriptorPoolSize{
              .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = static_cast<uint32_t>(swapchain.images.size()),
          },
      });

  auto renderPass = vuloxr::vk::createColorRenderPass(
      device, swapchain.createInfo.imageFormat);
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      //
      .setLayoutCount = 1,
      .pSetLayouts = &descriptorSets.descriptorSetLayout,
      //
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };
  VkPipelineLayout pipelineLayout;
  vuloxr::vk::CheckVkResult(vkCreatePipelineLayout(device, &pipelineLayoutInfo,
                                                   nullptr, &pipelineLayout));

  auto vs = vuloxr::vk::ShaderModule::createVertexShader(
      device, glsl_vs_to_spv(VS), "main");
  auto fs = vuloxr::vk::ShaderModule::createFragmentShader(
      device, glsl_fs_to_spv(FS), "main");

  vuloxr::vk::PipelineBuilder builder;
  builder.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  auto pipeline = builder.create(
      device, renderPass, pipelineLayout,
      {vs.pipelineShaderStageCreateInfo, fs.pipelineShaderStageCreateInfo},
      mesh.inputBindingDescriptions, mesh.attributeDescriptions, {}, {},
      {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

  std::vector<std::shared_ptr<vuloxr::vk::SwapchainFramebuffer>> backbuffers(
      swapchain.images.size());
  std::vector<std::shared_ptr<vko::Buffer>> uniformBuffers(
      swapchain.images.size());

  vuloxr::vk::FlightManager flightManager(
      device, physicalDevice.graphicsFamilyIndex, swapchain.images.size());

  while (runLoop()) {
    // * 1. Aquires an image from the swap chain
    auto acquireSemaphore = flightManager.getOrCreateSemaphore();
    auto [res, acquired] = swapchain.acquireNextImage(acquireSemaphore);
    if (res == VK_SUCCESS) {

      auto [cmd, flight] = flightManager.sync(acquireSemaphore);

      // * 2. Executes the  buffer with that image (as an attachment in
      // the framebuffer)
      auto descriptorSet = descriptorSets.descriptorSets[acquired.imageIndex];

      auto backbuffer = backbuffers[acquired.imageIndex];
      if (!backbuffer) {
        auto ubo = std::make_shared<vko::Buffer>(
            physicalDevice, device, sizeof(UniformBufferObject),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        uniformBuffers[acquired.imageIndex] = ubo;

        descriptorSets.update(acquired.imageIndex,
                              VkDescriptorBufferInfo{
                                  .buffer = ubo->buffer,
                                  .offset = 0,
                                  .range = sizeof(UniformBufferObject),
                              },
                              texture.descriptorInfo);

        backbuffer = std::make_shared<vuloxr::vk::SwapchainFramebuffer>(
            device, acquired.image, swapchain.createInfo.imageExtent,
            swapchain.createInfo.imageFormat, pipeline.renderPass);
        backbuffers[acquired.imageIndex] = backbuffer;

        {
          vuloxr::vk::RenderPassRecording recording(
              cmd, pipeline.renderPass, backbuffer->framebuffer,
              swapchain.createInfo.imageExtent, {0.0f, 0.0f, 0.0f, 1.0f},
              pipelineLayout, descriptorSet);
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
          if (mesh.vertexBuffer->buffer != VK_NULL_HANDLE) {
            VkBuffer vertexBuffers[] = {mesh.vertexBuffer->buffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
          }
          if (mesh.indexBuffer->buffer != VK_NULL_HANDLE) {
            vkCmdBindIndexBuffer(cmd, mesh.indexBuffer->buffer, 0,
                                 VK_INDEX_TYPE_UINT16);
          }
          vkCmdDrawIndexed(cmd, mesh.indexDrawCount, 1, 0, 0, 0);
        }
      }

      {
        // update ubo
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(
                         currentTime - startTime)
                         .count();
        UniformBufferObject ubo{};
        ubo.setTime(time, swapchain.createInfo.imageExtent.width,
                    swapchain.createInfo.imageExtent.height);
        uniformBuffers[acquired.imageIndex]->memory->assign(ubo);
      }

      vuloxr::vk::CheckVkResult(device.submit(
          cmd, acquireSemaphore, flight.submitSemaphore, flight.submitFence));

      // * 3. Returns the image to the swap chain for presentation.
      res = swapchain.present(acquired.imageIndex, flight.submitSemaphore);
    }

    // check if the swap chain is no longer adaquate for presentation
    if (res != VK_SUCCESS) {
      if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        vkDeviceWaitIdle(device);
        swapchain.create();
        backbuffers.clear();
        backbuffers.resize(swapchain.images.size());
        continue;
      }

      vuloxr::vk::CheckVkResult(res);
    }
  }

  vkDeviceWaitIdle(device);
}

#include "../main_loop.h"
#include <vuloxr/vk.h>
#include <vuloxr/vk/buffer.h>
#include <vuloxr/vk/command.h>
#include <vuloxr/vk/pipeline.h>
#include <vuloxr/vk/shaderc.h>

const char VS[] = {
#embed "texture.vert"
    , 0, 0, 0, 0};

const char FS[] = {
#embed "texture.frag"
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

void main_loop(const vuloxr::gui::WindowLoopOnce &windowLoopOnce,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *) {

  vuloxr::vk::Texture texture(device, 2, 2);
  texture.setMemory(physicalDevice.allocForTransfer(device, texture.image));

  {
    // upload image
    uint8_t pixels[] = {
        255, 0,   0,   255, // R
        0,   255, 0,   255, // G
        0,   0,   255, 255, // B
        255, 255, 255, 255, // WHITE
    };
    VkDeviceSize imageSize = 2 * 2 * 4;

    vuloxr::vk::Buffer stagingBuffer(device, imageSize,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto stagingMemory = physicalDevice.allocForMap(device, stagingBuffer);
    stagingMemory.mapWrite(pixels, imageSize);

    vuloxr::vk::CommandBufferPool pool(device,
                                       physicalDevice.graphicsFamilyIndex, 1);
    auto cmd = pool.commandBuffers[0];

    {
      vuloxr::vk::CommandScope(cmd)
          .transitionImageLayout(texture.image, VK_FORMAT_R8G8B8_SRGB,
                                 VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
          .copyBufferToImage(stagingBuffer, texture.image, 2, 2)
          .transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_SRGB,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    device.submit(cmd);
    vkQueueWaitIdle(device.queue);
  }

  Vertex vertices[] = {
      {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // top-left and RED
      {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},  // top-right and GREEN
      {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // bottom-right and BLUE
      {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}} // bottom-left and WHITE
  };
  vuloxr::vk::VertexBuffer vertexBuffer{
      .bindings =
          {
              {
                  .binding = 0,
                  .stride = static_cast<uint32_t>(sizeof(Vertex)),
                  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
              },
          },
      .attributes = {
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
  vertexBuffer.allocate(physicalDevice, device,
                        std::span<const Vertex>(vertices));

  vuloxr::vk::IndexBuffer indexBuffer(
      physicalDevice, device, std::span<const uint16_t>({0, 1, 2, 2, 3, 0}));

  vuloxr::vk::DescriptorSet descriptorSets(
      device, swapchain.images.size(),
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

  auto [renderPass, depthStencil] = vuloxr::vk::createColorRenderPass(
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
      device, vuloxr::vk::glsl_vs_to_spv(VS), "main");
  auto fs = vuloxr::vk::ShaderModule::createFragmentShader(
      device, vuloxr::vk::glsl_fs_to_spv(FS), "main");

  vuloxr::vk::PipelineBuilder builder;
  builder.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  auto pipeline = builder.create(
      device, renderPass, depthStencil, pipelineLayout,
      {vs.pipelineShaderStageCreateInfo, fs.pipelineShaderStageCreateInfo},
      vertexBuffer.bindings, vertexBuffer.attributes, {}, {},
      {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

  vuloxr::vk::SwapchainNoDepthFramebufferList backbuffers(
      device, renderPass, swapchain.createInfo.imageFormat);
  backbuffers.reset(swapchain.createInfo.imageExtent, swapchain.images);
  std::vector<std::shared_ptr<vuloxr::vk::UniformBuffer<UniformBufferObject>>>
      uniformBuffers(swapchain.images.size());

  vuloxr::vk::AcquireSemaphorePool semaphorePool(device);
  vuloxr::vk::CommandBufferPool pool(device, physicalDevice.graphicsFamilyIndex,
                                     swapchain.images.size());

  while (auto state = windowLoopOnce()) {
    // * 1. Aquires an image from the swap chain
    auto acquireSemaphore = semaphorePool.getOrCreate();
    auto [res, acquired] = swapchain.acquireNextImage(acquireSemaphore);
    if (res == VK_SUCCESS) {

      // * 2. Executes the  buffer with that image (as an attachment in
      // the framebuffer)
      // auto descriptorSet =
      // descriptorSets.descriptorSets[acquired.imageIndex];
      auto cmd = pool.commandBuffers[acquired.imageIndex];
      auto backbuffer = &backbuffers[acquired.imageIndex];
      semaphorePool.resetFenceAndMakePairSemaphore(backbuffer->submitFence,
                                                   acquireSemaphore);
      auto ubo = uniformBuffers[acquired.imageIndex];
      if (!ubo) {
        ubo = std::make_shared<vuloxr::vk::UniformBuffer<UniformBufferObject>>(
            device);
        ubo->memory = physicalDevice.allocForMap(device, ubo->buffer);
        uniformBuffers[acquired.imageIndex] = ubo;

        auto descriptorSet = descriptorSets.update(
            acquired.imageIndex,
            std::span<const vuloxr::vk::DescriptorUpdateInfo>({
                {
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = &ubo->info,
                },
                {
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &texture.descriptorInfo,
                },
            }));

        {
          VkClearValue clear[]{
              {.color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}}};
          vuloxr::vk::RenderPassRecording recording(
              cmd, pipelineLayout, pipeline.renderPass, backbuffer->framebuffer,
              swapchain.createInfo.imageExtent, clear, descriptorSet,
              VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

          indexBuffer.draw(cmd, pipeline, vertexBuffer.buffer);
        }
      }

      {
        // update ubo
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(
                         currentTime - startTime)
                         .count();
        ubo->value.setTime(time, swapchain.createInfo.imageExtent.width,
                           swapchain.createInfo.imageExtent.height);
        ubo->mapWrite();
      }

      vuloxr::vk::CheckVkResult(device.submit(cmd, acquireSemaphore,
                                              backbuffer->submitSemaphore,
                                              backbuffer->submitFence));

      // * 3. Returns the image to the swap chain for presentation.
      res = swapchain.present(acquired.imageIndex, backbuffer->submitSemaphore);
    }

    // check if the swap chain is no longer adaquate for presentation
    if (res != VK_SUCCESS) {
      if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        vkDeviceWaitIdle(device);
        swapchain.create();
        backbuffers.reset(swapchain.createInfo.imageExtent, swapchain.images);
        continue;
      }

      vuloxr::vk::CheckVkResult(res);
    }
  }

  vkDeviceWaitIdle(device);
}

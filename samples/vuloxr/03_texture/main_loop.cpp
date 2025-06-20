#include "../main_loop.h"
#include "../glsl_to_spv.h"
#include "scene.h"
#include "vuloxr/vk.h"
#include "vuloxr/vk/pipeline.h"
#include <vulkan/vulkan_core.h>

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

void main_loop(const std::function<bool()> &runLoop,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *) {

  Scene scene(physicalDevice, device, physicalDevice.graphicsFamilyIndex);

  VkQueue graphicsQueue;
  vkGetDeviceQueue(device, physicalDevice.graphicsFamilyIndex, 0,
                   &graphicsQueue);

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
      scene.mesh.inputBindingDescriptions, scene.mesh.attributeDescriptions);

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

        descriptorSets.update(
            acquired.imageIndex,
            VkDescriptorBufferInfo{
                .buffer = ubo->buffer,
                .offset = 0,
                .range = sizeof(UniformBufferObject),
            },
            VkDescriptorImageInfo{
                .sampler = scene.texture->sampler,
                .imageView = scene.texture->imageView,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            });

        backbuffer = std::make_shared<vuloxr::vk::SwapchainFramebuffer>(
            device, acquired.image, swapchain.createInfo.imageExtent,
            swapchain.createInfo.imageFormat, pipeline.renderPass);
        backbuffers[acquired.imageIndex] = backbuffer;

        {
          vuloxr::vk::RenderPassRecording recording(
              cmd, pipeline.renderPass, backbuffer->framebuffer,
              swapchain.createInfo.imageExtent, {0.0f, 0.0f, 0.0f, 1.0f},
              pipelineLayout, descriptorSet);
          // recording.drawIndexed(scene.mesh);
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

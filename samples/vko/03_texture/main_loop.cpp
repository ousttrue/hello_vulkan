#include "../main_loop.h"
#include "../glsl_to_spv.h"
#include "scene.h"
#include "vko/vko.h"
#include "vko/vko_pipeline.h"

auto VS = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main()
{
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0); 
    fragColor = inColor; 
    fragTexCoord = inTexCoord;
}
)";

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

auto FS = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(texSampler, fragTexCoord);
}
)";

void main_loop(const std::function<bool()> &runLoop,
               const vko::Surface &surface, vko::PhysicalDevice physicalDevice,
               const vko::Device &device) {

  Scene scene(physicalDevice, device, physicalDevice.graphicsFamilyIndex);

  VkQueue graphicsQueue;
  vkGetDeviceQueue(device, physicalDevice.graphicsFamilyIndex, 0,
                   &graphicsQueue);

  vko::Swapchain swapchain(device);
  swapchain.create(
      physicalDevice.physicalDevice, surface, surface.chooseSwapSurfaceFormat(),
      surface.chooseSwapPresentMode(), physicalDevice.graphicsFamilyIndex,
      physicalDevice.presentFamilyIndex, VK_NULL_HANDLE);
  auto imageCount = swapchain.images.size();

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
      imageCount, {
                      VkDescriptorPoolSize{
                          .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                          .descriptorCount = static_cast<uint32_t>(imageCount),
                      },
                      VkDescriptorPoolSize{
                          .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          .descriptorCount = static_cast<uint32_t>(imageCount),
                      },
                  });

  auto renderPass = vko::createSimpleRenderPass(
      device, surface.chooseSwapSurfaceFormat().format);
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
  VKO_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                   &pipelineLayout));

  auto vs =
      vko::ShaderModule::createVertexShader(device, glsl_vs_to_spv(VS), "main");
  auto fs = vko::ShaderModule::createFragmentShader(device, glsl_fs_to_spv(FS),
                                                    "main");

  vko::PipelineBuilder builder;
  builder.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  auto pipeline = builder.create(
      device, renderPass, pipelineLayout,
      {vs.pipelineShaderStageCreateInfo, fs.pipelineShaderStageCreateInfo},
      scene.vertexInputBindingDescriptions, scene.attributeDescriptions,
      {{
          .x = 0.0f,
          .y = 0.0f,
          .width = static_cast<float>(swapchain.createInfo.imageExtent.width),
          .height = static_cast<float>(swapchain.createInfo.imageExtent.height),
          .minDepth = 0.0f,
          .maxDepth = 1.0f,
      }},
      {{
          .offset = {0, 0},
          .extent = swapchain.createInfo.imageExtent,
      }});
  ;

  std::vector<std::shared_ptr<vko::SwapchainFramebuffer>> backbuffers(
      imageCount);
  std::vector<std::shared_ptr<vko::Buffer>> uniformBuffers(imageCount);

  vko::FlightManager flightManager(device, physicalDevice.graphicsFamilyIndex,
                                   imageCount);

  VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

  while (runLoop()) {
    // * 1. Aquires an image from the swap chain
    auto acquireSemaphore = flightManager.getOrCreateSemaphore();
    auto acquired = swapchain.acquireNextImage(acquireSemaphore);
    auto result = acquired.result;
    if (result == VK_SUCCESS) {

      auto [cmd, flight, oldSemaphore] = flightManager.sync(acquireSemaphore);
      if (oldSemaphore != VK_NULL_HANDLE) {
        flightManager.reuseSemaphore(oldSemaphore);
      }

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

        backbuffer = std::make_shared<vko::SwapchainFramebuffer>(
            device, acquired.image, swapchain.createInfo.imageExtent,
            swapchain.createInfo.imageFormat, pipeline.renderPass);
        backbuffers[acquired.imageIndex] = backbuffer;

        {
          vko::CommandBufferRecording recording(cmd, pipeline.renderPass,
                                                backbuffer->framebuffer, swapchain.createInfo.imageExtent, clearColor,
                                                pipelineLayout, descriptorSet);
          recording.drawIndexed(pipeline.graphicsPipeline, scene.indexDrawCount,
                                scene.vertexBuffer->buffer,
                                scene.indexBuffer->buffer);
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

      // submit command
      VkPipelineStageFlags waitStages[] = {
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      };
      VkSubmitInfo submitInfo{
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = &acquireSemaphore,
          .pWaitDstStageMask = waitStages,
          .commandBufferCount = 1,
          .pCommandBuffers = &cmd,
          // specify which semaphore to signal once command buffer has finished
          .signalSemaphoreCount = 1,
          .pSignalSemaphores = &flight.submitSemaphore,
      };
      if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, flight.submitFence) !=
          VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
      }

      // * 3. Returns the image to the swap chain for presentation.
      result = swapchain.present(acquired.imageIndex, flight.submitSemaphore);
    }

    // check if the swap chain is no longer adaquate for presentation
    if (result != VK_SUCCESS) {
      if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // https://developer.android.com/games/optimize/vulkan-prerotation
        // rotate device
        vkDeviceWaitIdle(device);
        // swapchain = Swapchain::createSwapchain(surface, res.physicalDevice,
        //                                        res.device,
        //                                        res.graphicsFamily,
        //                                        res.presetnFamily, swapchain);
        // swapchainCommand->createSwapchainDependent(
        //     swapchain->extent(), swapchain->imageCount(),
        //     pipeline->descriptorSetLayout());
        // pipeline->createGraphicsPipeline(swapchain->extent());
      } else {
        throw std::runtime_error("failed to aquire/prsent swap chain image!");
      }
    }
  }

  vkDeviceWaitIdle(device);
}

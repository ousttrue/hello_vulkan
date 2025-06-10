#include "../main_loop.h"
#include "pipeline_object.h"
#include "scene.h"
#include "vko/vko.h"
#include <glm/ext/matrix_projection.hpp>

static void bindTexture(VkDevice device,
                        const std::shared_ptr<vko::Buffer> &uniformBuffer,
                        VkImageView imageView, VkSampler sampler,
                        VkDescriptorSet descriptorSet) {
  VkDescriptorBufferInfo bufferInfo{
      .buffer = uniformBuffer->buffer,
      .offset = 0,
      .range = sizeof(UniformBufferObject),
  };
  VkDescriptorImageInfo imageInfo{
      .sampler = sampler,
      .imageView = imageView,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  VkWriteDescriptorSet descriptorWrites[2] = {
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = descriptorSet,
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .pBufferInfo = &bufferInfo,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = descriptorSet,
          .dstBinding = 1,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = &imageInfo,
      },
  };
  vkUpdateDescriptorSets(device,
                         static_cast<uint32_t>(std::size(descriptorWrites)),
                         descriptorWrites, 0, nullptr);
}

struct DescriptorCopy {
  VkDevice device;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> descriptorSets;

  DescriptorCopy(VkDevice _device, VkDescriptorSetLayout descriptorSetLayout,
                 uint32_t count)
      : device(_device) {
    VkDescriptorPoolSize poolSizes[] = {
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = static_cast<uint32_t>(count),
        },
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = static_cast<uint32_t>(count),
        },
    };
    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        // specifies the maximum number of descriptor sets that may be allocated
        .maxSets = count,
        .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
        .pPoolSizes = poolSizes,
    };
    VKO_CHECK(vkCreateDescriptorPool(_device, &poolInfo, nullptr,
                                     &this->descriptorPool));

    std::vector<VkDescriptorSetLayout> layouts(count, descriptorSetLayout);
    VkDescriptorSetAllocateInfo descriptorAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = this->descriptorPool,
        .descriptorSetCount = count,
        .pSetLayouts = layouts.data(),
    };
    this->descriptorSets.resize(count);
    VKO_CHECK(vkAllocateDescriptorSets(this->device, &descriptorAllocInfo,
                                       this->descriptorSets.data()));
  }
  ~DescriptorCopy() {
    // The descriptor pool should be destroyed here since it relies on the
    // number of images
    vkDestroyDescriptorPool(this->device, this->descriptorPool, nullptr);
  }
};

struct DescriptorSetLayout {
  VkDevice _device;
  VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;

  VkDescriptorSetLayoutBinding bindings[2] = {
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
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(std::size(bindings)),
      .pBindings = bindings,
  };

  // VkDescriptorSetLayout descriptorSetLayout;
  DescriptorSetLayout(VkDevice device) : _device(device) {
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                    &_descriptorSetLayout) != VK_SUCCESS) {
      throw std::runtime_error("failed to create descriptor set layout!");
    }
  }

  static std::shared_ptr<DescriptorSetLayout> create(VkDevice device) {
    auto ptr =
        std::shared_ptr<DescriptorSetLayout>(new DescriptorSetLayout(device));
    return ptr;
  }

  ~DescriptorSetLayout() {
    vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, nullptr);
  }
};

void main_loop(const std::function<bool()> &runLoop,
               const vko::Surface &surface, vko::PhysicalDevice physicalDevice,
               const vko::Device &device) {

  VkQueue graphicsQueue;
  vkGetDeviceQueue(device, physicalDevice.graphicsFamilyIndex, 0,
                   &graphicsQueue);

  vko::Swapchain swapchain(device);
  swapchain.create(
      physicalDevice.physicalDevice, surface, surface.chooseSwapSurfaceFormat(),
      surface.chooseSwapPresentMode(), physicalDevice.graphicsFamilyIndex,
      physicalDevice.presentFamilyIndex, VK_NULL_HANDLE);

  auto descriptorSetLayout = DescriptorSetLayout::create(device);

  PipelineObject pipeline(physicalDevice.physicalDevice, device,
                          physicalDevice.graphicsFamilyIndex,
                          surface.chooseSwapSurfaceFormat().format,
                          descriptorSetLayout->_descriptorSetLayout);

  Scene scene(physicalDevice, device, physicalDevice.graphicsFamilyIndex);

  DescriptorCopy descriptors(device, descriptorSetLayout->_descriptorSetLayout,
                             swapchain.images.size());

  std::vector<std::shared_ptr<vko::SwapchainFramebuffer>> backbuffers(
      swapchain.images.size());
  std::vector<std::shared_ptr<vko::Buffer>> uniformBuffers(
      swapchain.images.size());

  pipeline.createGraphicsPipeline(scene, swapchain.createInfo.imageExtent);

  vko::FlightManager flightManager(device, physicalDevice.graphicsFamilyIndex,
                                   swapchain.images.size());

  while (runLoop()) {
    // * 0.
    // auto flight = flightFrames->next();

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
      auto descriptorSet = descriptors.descriptorSets[acquired.imageIndex];

      auto backbuffer = backbuffers[acquired.imageIndex];
      if (!backbuffer) {
        auto ubo = std::make_shared<vko::Buffer>(
            physicalDevice, device, sizeof(UniformBufferObject),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        uniformBuffers[acquired.imageIndex] = ubo;

        // new backbuffer(framebuffer)
        backbuffer = std::make_shared<vko::SwapchainFramebuffer>(
            device, acquired.image, swapchain.createInfo.imageExtent,
            swapchain.createInfo.imageFormat, pipeline.renderPass());
        backbuffers[acquired.imageIndex] = backbuffer;

        bindTexture(device, ubo, scene.texture->imageView,
                    scene.texture->sampler, descriptorSet);

        pipeline.record(cmd, backbuffer->framebuffer,
                        swapchain.createInfo.imageExtent, descriptorSet, scene);
      }

      {
        // update ubo
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(
                         currentTime - startTime)
                         .count();
        // backbuffer->updateUbo(time, swapchain.createInfo.imageExtent);
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

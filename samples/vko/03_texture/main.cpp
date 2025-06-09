#include "glfw_app.h"
#include "per_image_object.h"
#include "pipeline_object.h"
#include "swapchain_command.h"
#include <concurrencysal.h>
#include <vko/vko.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <assert.h>
#include <chrono>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <vector>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

int main() {

  auto glfw = GlfwApp::initWindow(WIDTH, HEIGHT);
  auto instanceExtensions =
      GlfwApp::getRequiredExtensions(enableValidationLayers);

  vko::Instance instance;
  instance.validationLayers = validationLayers;
  instance.instanceExtensions = instanceExtensions;
  VKO_CHECK(instance.create());
  auto _surface = glfw->createSurface(instance);
  auto picked = instance.pickPhysicalDevice(_surface);
  vko::Surface surface(instance, _surface, picked.physicalDevice);

  vko::Device device;
  VKO_CHECK(device.create(picked.physicalDevice, picked.graphicsFamilyIndex,
                          picked.presentFamilyIndex));

  VkQueue graphicsQueue;
  vkGetDeviceQueue(device, picked.graphicsFamilyIndex, 0, &graphicsQueue);

  auto swapchainCommand = std::make_shared<SwapchainCommand>(
      picked.physicalDevice, device, picked.graphicsFamilyIndex);

  vko::Swapchain swapchain(device);
  swapchain.create(picked.physicalDevice, surface,
                   surface.chooseSwapSurfaceFormat(),
                   surface.chooseSwapPresentMode(), picked.graphicsFamilyIndex,
                   picked.presentFamilyIndex, VK_NULL_HANDLE);
  auto pipeline = PipelineObject::create(
      picked.physicalDevice, device, picked.graphicsFamilyIndex,
      surface.chooseSwapSurfaceFormat().format);

  swapchainCommand->createSwapchainDependent(swapchain.createInfo.imageExtent,
                                             swapchain.images.size(),
                                             pipeline->descriptorSetLayout());
  pipeline->createGraphicsPipeline(swapchain.createInfo.imageExtent);

  vko::FlightManager flightManager(device, picked.graphicsFamilyIndex,
                                   swapchain.images.size());

  while (glfw->running()) {
    // * 0.
    // auto flight = flightFrames->next();

    // * 1. Aquires an image from the swap chain
    auto acquireSemaphore = flightManager.getOrCreateSemaphore();
    auto acquired = swapchain.acquireNextImage(acquireSemaphore);
    auto result = acquired.result;
    if (result == VK_SUCCESS) {

      auto [_, flight, oldSemaphore] =
          flightManager.blockAndReset(acquireSemaphore);
      if (oldSemaphore != VK_NULL_HANDLE) {
        flightManager.reuseSemaphore(oldSemaphore);
      }

      // * 2. Executes the  buffer with that image (as an attachment in
      // the framebuffer)
      auto [commandBuffer, descriptorSet, backbuffer] =
          swapchainCommand->frameResource(acquired.imageIndex);

      if (!backbuffer) {
        // new backbuffer(framebuffer)
        backbuffer = swapchainCommand->createFrameResource(
            acquired.imageIndex, acquired.image,
            swapchain.createInfo.imageExtent, swapchain.createInfo.imageFormat,
            pipeline->renderPass());

        auto [imageView, sampler] = pipeline->texture();
        backbuffer->bindTexture(imageView, sampler);

        pipeline->record(commandBuffer, backbuffer->framebuffer(),
                         swapchain.createInfo.imageExtent, descriptorSet);
      }

      {
        // update ubo
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(
                         currentTime - startTime)
                         .count();
        backbuffer->updateUbo(time, swapchain.createInfo.imageExtent);
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
          .pCommandBuffers = &commandBuffer,
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

  return EXIT_SUCCESS;
}

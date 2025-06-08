#ifdef ANDROID
#else
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#include <vko/vko.h>

#include <chrono>
#include <cmath>

auto WINDOW_TITLE = "vko";

static void clearImage(VkCommandBuffer commandBuffer,
                       VkClearColorValue clearColorValue, VkImage image,
                       uint32_t graphicsQueueFamily) {

  VkCommandBufferBeginInfo CommandBufferBeginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
      .pInheritanceInfo = nullptr,
  };
  VKO_CHECK(vkBeginCommandBuffer(commandBuffer, &CommandBufferBeginInfo));

  VkImageSubresourceRange subResourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  };

  VkImageMemoryBarrier presentToClearBarrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .srcQueueFamilyIndex = graphicsQueueFamily,
      .dstQueueFamilyIndex = graphicsQueueFamily,
      .image = image,
      .subresourceRange = subResourceRange,
  };
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &presentToClearBarrier);

  VkImageSubresourceRange imageSubresourceRange{
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  };
  vkCmdClearColorImage(commandBuffer, image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue,
                       1, &imageSubresourceRange);

  VkImageMemoryBarrier clearToPresentBarrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .srcQueueFamilyIndex = graphicsQueueFamily,
      .dstQueueFamilyIndex = graphicsQueueFamily,
      .image = image,
      .subresourceRange = subResourceRange,
  };
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &clearToPresentBarrier);

  VKO_CHECK(vkEndCommandBuffer(commandBuffer));
}

VkClearColorValue getColorForTime(std::chrono::nanoseconds nano) {
  // auto sec = std::chrono::duration_cast<std::chrono::seconds>(nano);
  auto sec = std::chrono::duration_cast<std::chrono::duration<double>>(nano);
  const auto SPD = 3.0f;
  float v = (std::sin(sec.count() * SPD) + 1.0f) * 0.5;
  // std::cout << nano << ": " << std::fmod(sec.count(), 1.0) << ": "
  //           << std::sin(sec.count()) << ": " << v << std::endl;
  return {v, 0.0, 0.0, 0.0};
}

#ifdef ANDROID
#else

// https://github.com/ocornut/imgui/blob/master/examples/example_glfw_vulkan/main.cpp
int main(int argc, char **argv) {
  glfwSetErrorCallback([](int error, const char *description) {
    vko::Logger::Error("GLFW Error %d: %s\n", error, description);
  });
  if (!glfwInit()) {
    return 1;
  }
  // Create window with Vulkan context
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *window =
      glfwCreateWindow(1280, 720, WINDOW_TITLE, nullptr, nullptr);
  if (!glfwVulkanSupported()) {
    printf("GLFW: Vulkan Not Supported\n");
    return 1;
  }

  {
    //
    // vulkan instance with debug layer
    //
    vko::Instance instance;
    // vulkan extension for glfw surface
    uint32_t extensions_count = 0;
    auto glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
    for (uint32_t i = 0; i < extensions_count; i++) {
      instance.instanceExtensions.push_back(glfw_extensions[i]);
    }
#ifndef NDEBUG
    instance.validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    instance.instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    VKO_CHECK(instance.create());

    //
    // vulkan device with glfw surface
    //
    VkSurfaceKHR _surface;
    VKO_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &_surface));
    auto picked = instance.pickPhysicalDevice(_surface);
    vko::Surface surface(instance, _surface, picked.physicalDevice);

    vko::Device device;
    device.validationLayers = instance.validationLayers;
    VKO_CHECK(device.create(picked.physicalDevice, picked.graphicsFamilyIndex,
                            picked.presentFamilyIndex));

    vko::Swapchain swapchain(device);
    VKO_CHECK(swapchain.create(
        picked.physicalDevice, surface, surface.chooseSwapSurfaceFormat(),
        surface.chooseSwapPresentMode(), picked.graphicsFamilyIndex,
        picked.presentFamilyIndex));

    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, picked.graphicsFamilyIndex, 0, &graphicsQueue);

    std::vector<std::shared_ptr<vko::SwapchainFramebuffer>> backbuffers(
        swapchain.images.size());
    vko::FlightManager flightManager(device, picked.graphicsFamilyIndex,
                                     swapchain.images.size());

    //
    // main loop
    //
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      auto acquireSemaphore = flightManager.getOrCreateSemaphore();

      auto acquired = swapchain.acquireNextImage(acquireSemaphore);
      VKO_CHECK(acquired.result);

      auto [cmd, flight, oldSemaphore] =
          flightManager.blockAndReset(acquireSemaphore);
      if (oldSemaphore != VK_NULL_HANDLE) {
        flightManager.reuseSemaphore(oldSemaphore);
      }

      auto color =
          getColorForTime(std::chrono::nanoseconds(acquired.presentTimeNano));

      clearImage(cmd, color, acquired.image, picked.graphicsFamilyIndex);

      VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
      VkSubmitInfo submitInfo = {
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = &acquireSemaphore,
          .pWaitDstStageMask = &waitDstStageMask,
          .commandBufferCount = 1,
          .pCommandBuffers = &cmd,
          .signalSemaphoreCount = 1,
          .pSignalSemaphores = &flight.submitSemaphore,
      };

      VKO_CHECK(
          vkQueueSubmit(graphicsQueue, 1, &submitInfo, flight.submitFence));

      VKO_CHECK(swapchain.present(acquired.imageIndex, flight.submitSemaphore));
    }

    vkDeviceWaitIdle(device);
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
#endif

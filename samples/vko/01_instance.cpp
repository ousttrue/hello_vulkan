#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vko.h>

#include <chrono>
#include <cmath>
#include <vector>

auto WINDOW_TITLE = "vko";

class Renderer {
  VkDevice _device;
  VkCommandPool _commandPool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> _commandBuffers;

public:
  Renderer(VkDevice device, uint32_t queueFamilyIndex) : _device(device) {
    VkCommandPoolCreateInfo CommandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };
    VK_CHECK(vkCreateCommandPool(_device, &CommandPoolCreateInfo, nullptr,
                                 &_commandPool));

    _commandBuffers.resize(1);
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = _commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(_commandBuffers.size()),
    };
    VK_CHECK(vkAllocateCommandBuffers(_device, &CommandBufferAllocateInfo,
                                      _commandBuffers.data()));
  }
  ~Renderer() { vkDestroyCommandPool(_device, _commandPool, nullptr); }

  VkCommandBuffer getRecordedCommand(int64_t presentTimeNano, VkImage image,
                                     uint32_t graphicsQueueFamily) {
    auto commandBuffer = _commandBuffers[0];

    VkCommandBufferBeginInfo CommandBufferBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        .pInheritanceInfo = nullptr,
    };
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &CommandBufferBeginInfo));

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

    std::chrono::nanoseconds nano(presentTimeNano);
    // auto sec = std::chrono::duration_cast<std::chrono::seconds>(nano);
    auto sec = std::chrono::duration_cast<std::chrono::duration<double>>(nano);
    const auto SPD = 3.0f;
    float v = (std::sin(sec.count() * SPD) + 1.0f) * 0.5;
    // std::cout << nano << ": " << std::fmod(sec.count(), 1.0) << ": "
    //           << std::sin(sec.count()) << ": " << v << std::endl;
    VkClearColorValue clearColorValue = {v, 0.0, 0.0, 0.0};
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

    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    return commandBuffer;
  }
};

// https://github.com/ocornut/imgui/blob/master/examples/example_glfw_vulkan/main.cpp
int main(int argc, char **argv) {
  glfwSetErrorCallback([](int error, const char *description) {
    LOGE("GLFW Error %d: %s\n", error, description);
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
      instance._instanceExtensions.push_back(glfw_extensions[i]);
    }
#ifndef NDEBUG
    instance._validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    instance._instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    VK_CHECK(instance.create());

    //
    // vulkan device with glfw surface
    //
    VkSurfaceKHR _surface;
    VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &_surface));
    auto picked = instance.pickPhysicakDevice(_surface);
    vko::Surface surface(instance, _surface, picked.physicalDevice);

    vko::Device device;
    device._validationLayers = instance._validationLayers;
    VK_CHECK(device.create(picked.physicalDevice, picked.graphicsFamily,
                           picked.presentFamily));

    vko::Swapchain swapchain(device);
    VK_CHECK(swapchain.create(picked.physicalDevice, surface._surface,
                              surface.chooseSwapSurfaceFormat(),
                              surface.chooseSwapPresentMode(),
                              picked.graphicsFamily, picked.presentFamily));

    //
    // renderer
    //
    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, picked.graphicsFamily, 0, &graphicsQueue);

    Renderer renderer(device, picked.graphicsFamily);

    //
    // main loop
    //
    vko::Fence submitCompleteFence(device, true);

    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      auto acquired = swapchain.acquireNextImage();
      VK_CHECK(acquired.result);

      auto commandBuffer = renderer.getRecordedCommand(
          acquired.presentTimeNano, acquired.image, picked.graphicsFamily);

      VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
      VkSubmitInfo submitInfo = {
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = &acquired.imageAvailableSemaphore->_semaphore,
          .pWaitDstStageMask = &waitDstStageMask,
          .commandBufferCount = 1,
          .pCommandBuffers = &commandBuffer,
          .signalSemaphoreCount = 1,
          .pSignalSemaphores = &acquired.submitCompleteSemaphore->_semaphore,
      };
      submitCompleteFence.reset();
      VK_CHECK(
          vkQueueSubmit(graphicsQueue, 1, &submitInfo, submitCompleteFence));

      submitCompleteFence.block();
      VK_CHECK(swapchain.present(acquired.imageIndex,
                                 acquired.imageAvailableSemaphore));
    }

    vkDeviceWaitIdle(device);
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

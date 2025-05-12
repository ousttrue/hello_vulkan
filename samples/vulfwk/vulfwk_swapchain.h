#pragma once
#include <vulkan/vulkan.h>

#include <memory>
#include <optional>
#include <vector>

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities = {};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;

  bool querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
};

class SwapchainImpl {
public:
  static const int MAX_FRAMES_IN_FLIGHT = 2;

private:
  VkDevice Device;
  VkSwapchainKHR Swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> SwapchainImages;
  VkExtent2D SwapchainExtent = {0, 0};
  std::vector<VkImageView> SwapchainImageViews;
  std::vector<VkFramebuffer> SwapchainFramebuffers;

  std::vector<VkSemaphore> ImageAvailableSemaphores;
  std::vector<VkSemaphore> RenderFinishedSemaphores;
  std::vector<VkFence> InFlightFences;
  uint32_t CurrentFrame = 0;

  SwapchainImpl(VkDevice device, VkSwapchainKHR swapchain,
                VkExtent2D imageExtent, VkFormat format);

  bool createImageViews(VkFormat imageFormat);
  bool createFramebuffers(VkRenderPass renderPass);

public:
  ~SwapchainImpl();

  static VkSurfaceFormatKHR chooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR> &availableFormats);

  static std::shared_ptr<SwapchainImpl>
  create(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice device,
         VkExtent2D imageExtent, VkFormat imageFormat, VkRenderPass renderPass);

  bool isSameSize(uint32_t width, uint32_t height) const {
    return SwapchainExtent.width == width && SwapchainExtent.height == height;
  }
  VkExtent2D extent() const { return SwapchainExtent; }
  VkFramebuffer frameBuffer(uint32_t imageIndex) const {
    return SwapchainFramebuffers[imageIndex];
  }

  std::tuple<uint32_t, uint32_t> begin();
  bool end(uint32_t imageIndex, VkCommandBuffer commandBuffer,
           VkQueue graphicsQueue, VkQueue presentQueue);
};

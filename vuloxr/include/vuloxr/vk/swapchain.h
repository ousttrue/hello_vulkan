#pragma once
#include "buffer.h"
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <span>
#include <vector>
#include <vulkan/vulkan.h>

namespace vuloxr {
namespace vk {

inline std::optional<VkFormat>
selectColorSwapchainFormat(std::span<const int64_t> formats) {

  // List of supported color swapchain formats.
  constexpr VkFormat candidates[] = {
      VK_FORMAT_B8G8R8A8_SRGB,
      VK_FORMAT_R8G8B8A8_SRGB,
      VK_FORMAT_B8G8R8A8_UNORM,
      VK_FORMAT_R8G8B8A8_UNORM,
  };

  auto b = std::begin(candidates);
  auto e = std::end(candidates);

  std::optional<VkFormat> selected = {};
  std::string swapchainFormatsString;
  for (auto _format : formats) {
    auto format = (VkFormat)_format;

    auto found = std::find(b, e, format);
    swapchainFormatsString += " ";
    if (found != e) {
      swapchainFormatsString += "[";
    }
    swapchainFormatsString += magic_enum::enum_name(format);
    if (found != e) {
      swapchainFormatsString += "]";
    }

    if (!selected && found != e) {
      selected = *found;
    }
  }
  vuloxr::Logger::Info("Swapchain Formats: %s", swapchainFormatsString.c_str());

  return selected;
}

struct Swapchain : public NonCopyable {
  VkInstance instance = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  uint32_t presentFamily = UINT_MAX;
  VkDevice device = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  operator VkSwapchainKHR() const { return this->swapchain; }
  std::vector<VkImage> images;

  std::vector<VkSurfaceFormatKHR> formats;
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  VkQueue presentQueue;
  std::vector<VkPresentModeKHR> presentModes;
  uint32_t graphicsFamily;

  Swapchain() {}

  Swapchain(VkInstance _instance, VkSurfaceKHR _surface,
            VkPhysicalDevice _physicalDevice, uint32_t _presentFamily,
            VkDevice _device, uint32_t _graphicsFamily)
      : instance(_instance), surface(_surface), physicalDevice(_physicalDevice),
        presentFamily(_presentFamily), device(_device),
        graphicsFamily(_graphicsFamily) {
    move(nullptr);
  }

  ~Swapchain() {
    if (this->swapchain != VK_NULL_HANDLE) {
      Logger::Info("Swapchain::~Swapchain");
      vkDestroySwapchainKHR(this->device, this->swapchain, nullptr);
    }
    if (this->surface != VK_NULL_HANDLE) {
      vkDestroySurfaceKHR(this->instance, this->surface, nullptr);
    }
  }

  Swapchain(Swapchain &&rhs) {
    this->instance = rhs.instance;
    this->surface = rhs.surface;
    this->physicalDevice = rhs.physicalDevice;
    this->presentFamily = rhs.presentFamily;
    this->device = rhs.device;
    this->graphicsFamily = rhs.graphicsFamily;
    move(&rhs);
  }
  Swapchain &operator=(Swapchain &&rhs) {
    this->instance = rhs.instance;
    this->surface = rhs.surface;
    this->physicalDevice = rhs.physicalDevice;
    this->presentFamily = rhs.presentFamily;
    this->device = rhs.device;
    this->graphicsFamily = rhs.graphicsFamily;
    move(&rhs);
    return *this;
  }
  void move(Swapchain *src) {
    if (src) {
      this->createInfo = src->createInfo;
      this->swapchain = src->swapchain;
      src->swapchain = VK_NULL_HANDLE;
      this->images.swap(src->images);
      src->surface = VK_NULL_HANDLE;
    }
    if (this->device == VK_NULL_HANDLE) {
      return;
    }

    vkGetDeviceQueue(this->device, this->presentFamily, 0, &this->presentQueue);
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        this->physicalDevice, this->surface, &this->surfaceCapabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, this->surface,
                                         &formatCount, nullptr);
    if (formatCount != 0) {
      this->formats.resize(formatCount);
      vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, this->surface,
                                           &formatCount, this->formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, this->surface,
                                              &presentModeCount, nullptr);
    if (presentModeCount != 0) {
      this->presentModes.resize(presentModeCount);
      vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, this->surface,
                                                &presentModeCount,
                                                this->presentModes.data());
    }
  }

  VkSurfaceFormatKHR chooseSwapSurfaceFormat() const {
    if (this->formats.empty()) {
      return {
          .format = VK_FORMAT_UNDEFINED,
      };
    }
    const VkFormat requestSurfaceImageFormat[] = {
        // Favor UNORM formats as the samples are not written for sRGB
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM,
        // VK_FORMAT_A8B8G8R8_UNORM_PACK32
        // VK_FORMAT_B8G8R8A8_SRGB,
    };
    const VkColorSpaceKHR requestSurfaceColorSpace =
        VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    for (const auto &availableFormat : this->formats) {
      if (availableFormat.colorSpace == requestSurfaceColorSpace) {
        for (auto format : requestSurfaceImageFormat) {
          if (availableFormat.format == format) {
            return availableFormat;
          }
        }
      }
    }
    return this->formats[0];
  }
  VkPresentModeKHR chooseSwapPresentMode() const {
#ifdef ANDROID
#else
    // for (const auto &availablePresentMode : this->presentModes) {
    //   if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
    //     return availablePresentMode;
    //   }
    // }

#endif
    // VSYNC ?
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  uint32_t queueFamilyIndices[2] = {};
  VkSwapchainCreateInfoKHR createInfo{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .imageArrayLayers = 1,
      .imageUsage =
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,

  // Find a supported compositeAlpha type.
  // VkCompositeAlphaFlagBitsKHR compositeAlpha =
  // VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; if
  // (surfaceProperties.supportedCompositeAlpha &
  //     VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
  //   compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  // else if (surfaceProperties.supportedCompositeAlpha &
  //          VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
  //   compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
  // else if (surfaceProperties.supportedCompositeAlpha &
  //          VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
  //   compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
  // else if (surfaceProperties.supportedCompositeAlpha &
  //          VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
  //   compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
#ifdef ANDROID
      .compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
#else
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
#endif
      .clipped = VK_TRUE,
  };

  VkResult create() {

    this->createInfo.surface = this->surface;
    // Determine the number of VkImage's to use in the swapchain.
    // Ideally, we desire to own 1 image at a time, the rest of the images can
    // either be rendered to and/or
    // being queued up for display.
    // uint32_t desiredSwapchainImages = surfaceProperties.minImageCount + 1;
    // if ((surfaceProperties.maxImageCount > 0) &&
    //     (desiredSwapchainImages > surfaceProperties.maxImageCount)) {
    //   // Application must settle for fewer images than desired.
    //   desiredSwapchainImages = surfaceProperties.maxImageCount;
    // }

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        this->physicalDevice, this->surface, &this->surfaceCapabilities);
    this->createInfo.minImageCount = this->surfaceCapabilities.minImageCount;
    this->createInfo.imageExtent = this->surfaceCapabilities.currentExtent;
    this->createInfo.preTransform = this->surfaceCapabilities.currentTransform;

    auto surfaceFormat = chooseSwapSurfaceFormat();
    this->createInfo.imageFormat = surfaceFormat.format;
    this->createInfo.imageColorSpace = surfaceFormat.colorSpace;

    if (graphicsFamily == this->presentFamily) {
      this->createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      this->createInfo.queueFamilyIndexCount = 0;     // Optional
      this->createInfo.pQueueFamilyIndices = nullptr; // Optional
    } else {
      this->createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      this->createInfo.queueFamilyIndexCount = 2;
      this->queueFamilyIndices[0] = graphicsFamily;
      this->queueFamilyIndices[1] = presentFamily;
      this->createInfo.pQueueFamilyIndices = this->queueFamilyIndices;
    }
    // Figure out a suitable surface transform.
    // VkSurfaceTransformFlagBitsKHR preTransform;
    // if (surfaceProperties.supportedTransforms &
    //     VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    //   preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    // else
    //   preTransform = surfaceProperties.currentTransform;
    this->createInfo.presentMode = chooseSwapPresentMode();

    auto oldSwapchain = this->swapchain;
    this->createInfo.oldSwapchain = oldSwapchain;

    auto result = vkCreateSwapchainKHR(this->device, &this->createInfo, nullptr,
                                       &this->swapchain);

    if (oldSwapchain != VK_NULL_HANDLE) {
      Logger::Info("Swapchain::~Swapchain");
      vkDestroySwapchainKHR(this->device, oldSwapchain, nullptr);
    }

    if (result != VK_SUCCESS) {
      return result;
    }

    Logger::Info("swapchain.extent: %d x %d",
                 this->createInfo.imageExtent.width,
                 this->createInfo.imageExtent.height);
    Logger::Info("swapchain.format: %s",
                 magic_enum::enum_name(this->createInfo.imageFormat).data());
    Logger::Info(
        "swapchain.imageSharingMode: %s",
        magic_enum::enum_name(this->createInfo.imageSharingMode).data());
    Logger::Info("swapchain.presentMode: %s",
                 magic_enum::enum_name(this->createInfo.presentMode).data());

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(this->device, this->swapchain, &imageCount,
                            nullptr);
    Logger::Info("swapchain images: %d", imageCount);
    if (imageCount > 0) {
      this->images.resize(imageCount);
      vkGetSwapchainImagesKHR(this->device, this->swapchain, &imageCount,
                              this->images.data());
    }

    return VK_SUCCESS;
  }

  struct AcquiredImage {
    int64_t presentTimeNano;
    uint32_t imageIndex;
    VkImage image;
  };

  std::tuple<VkResult, AcquiredImage>
  acquireNextImage(VkSemaphore imageAvailableSemaphore) {
    uint32_t imageIndex;
    auto result = vkAcquireNextImageKHR(this->device, this->swapchain,
                                        UINT64_MAX, imageAvailableSemaphore,
                                        VK_NULL_HANDLE, &imageIndex);
    if (result != VK_SUCCESS) {
      return {result, {}};
    }

    auto now = std::chrono::system_clock::now();
    auto epoch_time = now.time_since_epoch();
    auto epoch_time_nano =
        std::chrono::duration_cast<std::chrono::nanoseconds>(epoch_time)
            .count();

    return {
        result,
        {
            epoch_time_nano,
            imageIndex,
            this->images[imageIndex],
        },
    };
  }

  VkResult present(uint32_t imageIndex,
                   VkSemaphore submitSemaphore = VK_NULL_HANDLE) {
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        // swapchain
        .swapchainCount = 1,
        .pSwapchains = &this->swapchain,
        .pImageIndices = &imageIndex,
    };
    if (submitSemaphore != VK_NULL_HANDLE) {
      presentInfo.waitSemaphoreCount = 1;
      presentInfo.pWaitSemaphores = &submitSemaphore;
    }
    return vkQueuePresentKHR(this->presentQueue, &presentInfo);
  }
};

struct SwapchainNoDepthFramebufferList : NonCopyable {
  // static
  VkDevice device;
  VkFormat format;

  // mutable
  struct Framebuffer {
    VkImageView imageView = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
  };
  std::vector<Framebuffer> framebuffers;

  SwapchainNoDepthFramebufferList(VkDevice _device, VkFormat _format)
      : device(_device), format(_format) {}

  ~SwapchainNoDepthFramebufferList() { release(); }

  void release() {
    for (auto &framebuffer : this->framebuffers) {
      vkDestroyFramebuffer(this->device, framebuffer.framebuffer, nullptr);
      vkDestroyImageView(this->device, framebuffer.imageView, nullptr);
    }
    this->framebuffers.clear();
  }

  bool empty() const { return this->framebuffers.empty(); }
  const Framebuffer &operator[](uint32_t index) const {
    return this->framebuffers[index];
  }

  void reset(VkRenderPass renderPass, VkExtent2D extent,
             std::span<const VkImage> images) {
    release();

    this->framebuffers.resize(images.size());
    for (int i = 0; i < images.size(); ++i) {
      auto image = images[i];
      auto framebuffer = &this->framebuffers[i];

      VkImageViewCreateInfo imageViewCreateInfo{
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .image = image,
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = this->format,
          .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .a = VK_COMPONENT_SWIZZLE_IDENTITY},
          .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                               .baseMipLevel = 0,
                               .levelCount = 1,
                               .baseArrayLayer = 0,
                               .layerCount = 1},
      };
      vuloxr::vk::CheckVkResult(vkCreateImageView(this->device,
                                                  &imageViewCreateInfo, nullptr,
                                                  &framebuffer->imageView));

      VkFramebufferCreateInfo framebufferInfo{
          .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
          .renderPass = renderPass,
          .attachmentCount = 1,
          .pAttachments = &framebuffer->imageView,
          .width = extent.width,
          .height = extent.height,
          .layers = 1,
      };
      vuloxr::vk::CheckVkResult(vkCreateFramebuffer(
          this->device, &framebufferInfo, nullptr, &framebuffer->framebuffer));
    }
  }
};

struct SwapchainSharedDepthFramebufferList : NonCopyable {
  VkDevice device;
  VkFormat format;
  VkFormat depthFormat;
  VkSampleCountFlagBits sampleCountFlagBits;

  DepthImage depth;
  VkImageView depthView = VK_NULL_HANDLE;

  struct Framebuffer {
    VkImageView imageView = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
  };
  std::vector<Framebuffer> framebuffers;

  SwapchainSharedDepthFramebufferList(
      VkDevice _device, VkFormat _format, VkFormat _depthFormat,
      VkSampleCountFlagBits _sampleCountFlagBits)
      : device(_device), format(_format), depthFormat(_depthFormat),
        sampleCountFlagBits(_sampleCountFlagBits) {}

  ~SwapchainSharedDepthFramebufferList() { release(); }

  void release() {
    for (auto &framebuffer : this->framebuffers) {
      vkDestroyFramebuffer(this->device, framebuffer.framebuffer, nullptr);
      vkDestroyImageView(this->device, framebuffer.imageView, nullptr);
    }
    this->framebuffers.clear();
    vkDestroyImageView(this->device, this->depthView, nullptr);
    this->depth = {};
  }
  const Framebuffer &operator[](uint32_t index) const {
    return this->framebuffers[index];
  }

  void reset(const PhysicalDevice &physicalDevice, VkRenderPass renderPass,
             VkExtent2D extent, std::span<const VkImage> images) {
    release();

    this->depth = vuloxr::vk::DepthImage(
        this->device, extent, this->depthFormat, this->sampleCountFlagBits);
    this->depth.memory =
        physicalDevice.allocForTransfer(device, this->depth.image);

    VkImageViewCreateInfo depthViewCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = this->depth.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = this->depthFormat,
        .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .a = VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1},
    };
    vuloxr::vk::CheckVkResult(vkCreateImageView(
        this->device, &depthViewCreateInfo, nullptr, &this->depthView));

    this->framebuffers.resize(images.size());
    for (int i = 0; i < images.size(); ++i) {
      auto image = images[i];
      auto framebuffer = &this->framebuffers[i];

      VkImageViewCreateInfo imageViewCreateInfo{
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .image = image,
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = this->format,
          .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .a = VK_COMPONENT_SWIZZLE_IDENTITY},
          .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                               .baseMipLevel = 0,
                               .levelCount = 1,
                               .baseArrayLayer = 0,
                               .layerCount = 1},
      };
      vuloxr::vk::CheckVkResult(vkCreateImageView(this->device,
                                                  &imageViewCreateInfo, nullptr,
                                                  &framebuffer->imageView));

      VkImageView attachments[] = {framebuffer->imageView, this->depthView};
      VkFramebufferCreateInfo framebufferInfo{
          .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
          .renderPass = renderPass,
          .attachmentCount = static_cast<uint32_t>(std::size(attachments)),
          .pAttachments = attachments,
          .width = extent.width,
          .height = extent.height,
          .layers = 1,
      };
      vuloxr::vk::CheckVkResult(vkCreateFramebuffer(
          this->device, &framebufferInfo, nullptr, &framebuffer->framebuffer));
    }
  }
};

struct SwapchainIsolatedDepthFramebufferList : NonCopyable {
  VkDevice device;
  VkFormat format;
  VkFormat depthFormat;
  VkSampleCountFlagBits sampleCountFlagBits;

  struct Framebuffer {
    VkImageView imageView = VK_NULL_HANDLE;
    DepthImage depth;
    VkImageView depthView = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
  };
  std::vector<Framebuffer> framebuffers;

  SwapchainIsolatedDepthFramebufferList(
      VkDevice _device, VkFormat _format, VkFormat _depthFormat,
      VkSampleCountFlagBits _sampleCountFlagBits)
      : device(_device), format(_format), depthFormat(_depthFormat),
        sampleCountFlagBits(_sampleCountFlagBits) {}

  ~SwapchainIsolatedDepthFramebufferList() { release(); }

  void release() {
    for (auto &framebuffer : this->framebuffers) {
      vkDestroyFramebuffer(this->device, framebuffer.framebuffer, nullptr);
      vkDestroyImageView(this->device, framebuffer.imageView, nullptr);
      vkDestroyImageView(this->device, framebuffer.depthView, nullptr);
    }
    this->framebuffers.clear();
  }

  SwapchainIsolatedDepthFramebufferList(
      SwapchainIsolatedDepthFramebufferList &&rhs) {
    release();
    this->device = rhs.device;
    this->format = rhs.format;
    this->depthFormat = rhs.depthFormat;
    this->sampleCountFlagBits = rhs.sampleCountFlagBits;
    std::swap(this->framebuffers, rhs.framebuffers);
  }

  SwapchainIsolatedDepthFramebufferList &
  operator=(SwapchainIsolatedDepthFramebufferList &&rhs) {
    release();
    this->device = rhs.device;
    this->format = rhs.format;
    this->depthFormat = rhs.depthFormat;
    this->sampleCountFlagBits = rhs.sampleCountFlagBits;
    std::swap(this->framebuffers, rhs.framebuffers);
    return *this;
  }

  const Framebuffer &operator[](uint32_t index) const {
    return this->framebuffers[index];
  }

  void reset(const PhysicalDevice &physicalDevice, VkRenderPass renderPass,
             VkExtent2D extent, std::span<const VkImage> images) {
    release();
    this->framebuffers.resize(images.size());
    for (int i = 0; i < images.size(); ++i) {
      auto image = images[i];
      auto framebuffer = &this->framebuffers[i];

      VkImageViewCreateInfo imageViewCreateInfo{
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .image = image,
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = this->format,
          .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .a = VK_COMPONENT_SWIZZLE_IDENTITY},
          .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                               .baseMipLevel = 0,
                               .levelCount = 1,
                               .baseArrayLayer = 0,
                               .layerCount = 1},
      };
      vuloxr::vk::CheckVkResult(vkCreateImageView(this->device,
                                                  &imageViewCreateInfo, nullptr,
                                                  &framebuffer->imageView));

      framebuffer->depth = vuloxr::vk::DepthImage(
          this->device, extent, this->depthFormat, this->sampleCountFlagBits);
      framebuffer->depth.memory =
          physicalDevice.allocForTransfer(device, framebuffer->depth.image);

      VkImageViewCreateInfo depthViewCreateInfo{
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .image = framebuffer->depth.image,
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = this->depthFormat,
          .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                         .a = VK_COMPONENT_SWIZZLE_IDENTITY},
          .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                               .baseMipLevel = 0,
                               .levelCount = 1,
                               .baseArrayLayer = 0,
                               .layerCount = 1},
      };
      vuloxr::vk::CheckVkResult(vkCreateImageView(this->device,
                                                  &depthViewCreateInfo, nullptr,
                                                  &framebuffer->depthView));

      VkImageView attachments[] = {framebuffer->imageView,
                                   framebuffer->depthView};
      VkFramebufferCreateInfo framebufferInfo{
          .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
          .renderPass = renderPass,
          .attachmentCount = static_cast<uint32_t>(std::size(attachments)),
          .pAttachments = attachments,
          .width = extent.width,
          .height = extent.height,
          .layers = 1,
      };
      vuloxr::vk::CheckVkResult(vkCreateFramebuffer(
          this->device, &framebufferInfo, nullptr, &framebuffer->framebuffer));
    }
  }
};

struct Vulkan {
  Instance instance;
  PhysicalDevice physicalDevice;
  Device device;
  Swapchain swapchain;
};

} // namespace vk
} // namespace vuloxr

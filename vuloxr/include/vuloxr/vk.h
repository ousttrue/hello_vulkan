#pragma once
#include "vuloxr.h"
#include <assert.h>
#include <chrono>
#include <climits>
#include <cstdint>
#include <magic_enum/magic_enum.hpp>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace vuloxr {

namespace vk {

[[noreturn]] inline void ThrowVkResult(VkResult res,
                                       const char *originator = nullptr,
                                       const char *sourceLocation = nullptr) {
  Throw(std::string("VrResult failure [%s]", magic_enum::enum_name(res).data()),
        originator, sourceLocation);
}

inline VkResult CheckVkResult(VkResult res, const char *originator = nullptr,
                              const char *sourceLocation = nullptr) {
  if (VK_SUCCESS != res) {
    ThrowVkResult(res, originator, sourceLocation);
  }
  return res;
}

struct Platform {
  std::vector<VkLayerProperties> layerProperties;
  std::vector<VkExtensionProperties> instanceExtensionProperties;

private:
  Platform() {
    // Enumerate available layers
    uint32_t layerCount;
    CheckVkResult(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));
    layerProperties.resize(layerCount);
    CheckVkResult(vkEnumerateInstanceLayerProperties(&layerCount,
                                                     layerProperties.data()));

    // Enumerate available extensions
    uint32_t extensionCount;
    CheckVkResult(vkEnumerateInstanceExtensionProperties(
        nullptr, &extensionCount, nullptr));
    this->instanceExtensionProperties.resize(extensionCount);
    CheckVkResult(vkEnumerateInstanceExtensionProperties(
        nullptr, &extensionCount, this->instanceExtensionProperties.data()));
  }

public:
  static Platform &singleton() {
    static Platform s_platform;
    return s_platform;
  }

  bool isLayerAvailable(const char *layer) {
    for (auto &p : this->layerProperties)
      if (strcmp(p.layerName, layer) == 0)
        return true;
    return false;
  }

  bool isInstanceExtensionAvailable(const char *extension) {
    for (auto &p : this->instanceExtensionProperties)
      if (strcmp(p.extensionName, extension) == 0)
        return true;
    return false;
  }
};

struct PhysicalDevice {
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  operator VkPhysicalDevice() const { return this->physicalDevice; }
  std::vector<VkExtensionProperties> deviceExtensionProperties;

  VkPhysicalDeviceProperties properties = {};
  VkPhysicalDeviceFeatures features = {};
  std::vector<VkQueueFamilyProperties> queueFamilyProperties;
  uint32_t graphicsFamilyIndex = UINT_MAX;

  // PhysicalDevice() {}
  PhysicalDevice(VkPhysicalDevice _physicalDevice)
      : physicalDevice(_physicalDevice) {
    vkGetPhysicalDeviceProperties(this->physicalDevice, &this->properties);
    vkGetPhysicalDeviceFeatures(this->physicalDevice, &this->features);
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(this->physicalDevice,
                                             &queueFamilyCount, nullptr);
    this->queueFamilyProperties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        this->physicalDevice, &queueFamilyCount,
        this->queueFamilyProperties.data());

    for (uint32_t i = 0; i < this->queueFamilyProperties.size(); ++i) {
      if (this->queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        this->graphicsFamilyIndex = i;
        break;
      }
    }

    // Enumerate physical device extension
    uint32_t properties_count;
    vkEnumerateDeviceExtensionProperties(this->physicalDevice, nullptr,
                                         &properties_count, nullptr);
    this->deviceExtensionProperties.resize(properties_count);
    vkEnumerateDeviceExtensionProperties(
        this->physicalDevice, nullptr, &properties_count,
        this->deviceExtensionProperties.data());
  }
  ~PhysicalDevice() {}

  bool isDeviceExtensionAvailable(const char *extension) const {
    for (const VkExtensionProperties &p : this->deviceExtensionProperties)
      if (strcmp(p.extensionName, extension) == 0)
        return true;
    return false;
  }

  std::optional<uint32_t>
  getFirstPresentQueueFamily(VkSurfaceKHR surface) const {
    for (uint32_t i = 0; i < this->queueFamilyProperties.size(); ++i) {
      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(this->physicalDevice, i, surface,
                                           &presentSupport);
      if (presentSupport) {
        return i;
      }
    }
    return {};
  }

  void debugPrint(VkSurfaceKHR surface) const {
    Logger::Info("[%s] %s", this->properties.deviceName,
                 magic_enum::enum_name(this->properties.deviceType).data());
    Logger::Info(
        "  queue info: "
        "present,graphics,compute,transfer,sparse,protected,video_de,video_en,"
        "optical"

    );
    for (int i = 0; i < this->queueFamilyProperties.size(); ++i) {
      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(this->physicalDevice, i, surface,
                                           &presentSupport);
      Logger::Info(
          "  [%02d] %s%s", i, (presentSupport ? "o" : "_"),
          queueFlagBitsStr(this->queueFamilyProperties[i].queueFlags, "o", "_")
              .c_str());
    }
  }
  static std::string queueFlagBitsStr(VkQueueFlags f, const char *enable,
                                      const char *disable) {
    std::string str;
    str += f & VK_QUEUE_GRAPHICS_BIT ? enable : disable;
    str += f & VK_QUEUE_COMPUTE_BIT ? enable : disable;
    str += f & VK_QUEUE_TRANSFER_BIT ? enable : disable;
    str += f & VK_QUEUE_SPARSE_BINDING_BIT ? enable : disable;
    str += f & VK_QUEUE_PROTECTED_BIT ? enable : disable;
    str += f & VK_QUEUE_VIDEO_DECODE_BIT_KHR ? enable : disable;
    str += f & VK_QUEUE_VIDEO_ENCODE_BIT_KHR ? enable : disable;
    str += f & VK_QUEUE_OPTICAL_FLOW_BIT_NV ? enable : disable;
    return str;
  }
};

struct Instance : NonCopyable {
  std::vector<const char *> layers;
  bool addLayer(const char *layer) {
    if (Platform::singleton().isLayerAvailable(layer)) {
      this->layers.push_back(layer);
      return true;
    } else {
      return false;
    }
  }

  std::vector<const char *> extensions;
  bool addExtension(const char *extension) {
    if (Platform::singleton().isInstanceExtensionAvailable(extension)) {
      this->extensions.push_back(extension);
      return true;
    } else {
      return false;
    }
  }

  VkInstanceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
  };
  VkInstance instance = VK_NULL_HANDLE;
  operator VkInstance() const { return this->instance; }
  std::vector<PhysicalDevice> physicalDevices;

  // VK_EXT_debug_report
  VkDebugReportCallbackEXT debugReport = VK_NULL_HANDLE;
  VkDebugReportCallbackCreateInfoEXT debug_report_ci = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
      .flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
               VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,
      .pfnCallback = debug_report,
      .pUserData = nullptr,
  };
  static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(
      VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
      uint64_t object, size_t location, int32_t messageCode,
      const char *pLayerPrefix, const char *pMessage, void *pUserData) {
    (void)flags;
    (void)object;
    (void)location;
    (void)messageCode;
    (void)pUserData;
    (void)pLayerPrefix; // Unused arguments
    fprintf(stderr,
            "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n",
            objectType, pMessage);
    return VK_FALSE;
  }

  Instance() {}
  ~Instance() {
    if (this->debugReport != VK_NULL_HANDLE) {
      // Remove the debug report callback
      auto f_vkDestroyDebugReportCallbackEXT =
          (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
              instance, "vkDestroyDebugReportCallbackEXT");
      f_vkDestroyDebugReportCallbackEXT(instance, this->debugReport, nullptr);
    }
    if (this->instance != VK_NULL_HANDLE) {
      Logger::Info("Instance::~Instance: %x", this->instance);
      vkDestroyInstance(instance, nullptr);
    }
  }
  Instance(Instance &&rhs) {
    this->debugReport = rhs.debugReport;
    rhs.debugReport = VK_NULL_HANDLE;
    this->reset(rhs.instance);
    rhs.instance = VK_NULL_HANDLE;
  }
  Instance &operator=(Instance &&rhs) {
    this->debugReport = rhs.debugReport;
    rhs.debugReport = VK_NULL_HANDLE;
    this->reset(rhs.instance);
    rhs.instance = VK_NULL_HANDLE;
    return *this;
  }

  VkResult create() {
    addExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    create_info.enabledLayerCount =
        static_cast<uint32_t>(std::size(this->layers));
    create_info.ppEnabledLayerNames = this->layers.data();
    create_info.enabledExtensionCount =
        static_cast<uint32_t>(std::size(this->extensions));
    create_info.ppEnabledExtensionNames = this->extensions.data();

    VkInstance instance;
    auto err = vkCreateInstance(&this->create_info, nullptr /*g_Allocator*/,
                                &instance);
    CheckVkResult(err);
    reset(instance);

    return VK_SUCCESS;
  }

  void reset(VkInstance _instance) {
    assert(this->instance == VK_NULL_HANDLE);
    this->instance = _instance;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(this->instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
      Logger::Error("no physical device\n");
    } else {
      std::vector<VkPhysicalDevice> devices(deviceCount);
      CheckVkResult(vkEnumeratePhysicalDevices(this->instance, &deviceCount,
                                               devices.data()));
      for (auto d : devices) {
        this->physicalDevices.push_back(PhysicalDevice(d));
      }
    }

    // Setup the debug report callback
    if (find_name(this->extensions, "VK_EXT_debug_report") &&
        this->debugReport == VK_NULL_HANDLE) {
      auto f_vkCreateDebugReportCallbackEXT =
          (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
              this->instance, "vkCreateDebugReportCallbackEXT");
      assert(f_vkCreateDebugReportCallbackEXT);
      CheckVkResult(f_vkCreateDebugReportCallbackEXT(
          this->instance, &this->debug_report_ci, nullptr /*g_Allocator*/,
          &this->debugReport));
    }
  }

  std::tuple<const PhysicalDevice *, std::optional<uint32_t>>
  pickPhysicalDevice(VkSurfaceKHR surface) const {
    const PhysicalDevice *picked = nullptr;
    std::optional<uint32_t> presentFamily;
    for (auto &physicalDevice : this->physicalDevices) {
      physicalDevice.debugPrint(surface);
      if (auto _presentFamily =
              physicalDevice.getFirstPresentQueueFamily(surface)) {
        if (!picked) {
          // use 1st
          picked = &physicalDevice;
          presentFamily = _presentFamily;
        }
      }
    }
    if (this->physicalDevices.size() > 0) {
      // fall back. use 1st device
      picked = &this->physicalDevices[0];
    }
    return {picked, presentFamily};
  }
};

struct Device : NonCopyable {
  std::vector<const char *> layers;

  std::vector<const char *> extensions;
  bool addExtension(const PhysicalDevice &physicalDevice,
                    const char *extension) {
    if (physicalDevice.isDeviceExtensionAvailable(extension)) {
      this->extensions.push_back(extension);
      return true;
    } else {
      return false;
    }
  }

  VkDevice device = VK_NULL_HANDLE;
  operator VkDevice() const { return this->device; }
  uint32_t queueFamily = UINT_MAX;
  VkQueue queue = VK_NULL_HANDLE;

  Device() {}
  ~Device() {
    if (this->device != VK_NULL_HANDLE) {
      Logger::Info("Device::~Device: %x", this->device);
      vkDestroyDevice(this->device, nullptr);
    }
  }
  Device(Device &&rhs) {
    reset(rhs.device, rhs.queueFamily);
    rhs.device = VK_NULL_HANDLE;
  }
  Device &operator=(Device &&rhs) {
    reset(rhs.device, rhs.queueFamily);
    rhs.device = VK_NULL_HANDLE;
    return *this;
  }

  // Create Logical Device (with 1 queue)
  VkResult create(VkInstance instance, const PhysicalDevice &physicalDevice,
                  uint32_t queueFamily) {
    const float queue_priority[] = {1.0f};
    VkDeviceQueueCreateInfo queue_info[1] = {};
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = queueFamily;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = queue_priority;
    VkPhysicalDeviceFeatures features{
        .samplerAnisotropy = VK_TRUE,
    };
    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pEnabledFeatures = &features,
    };
    create_info.queueCreateInfoCount =
        sizeof(queue_info) / sizeof(queue_info[0]);
    create_info.pQueueCreateInfos = queue_info;
    create_info.enabledExtensionCount =
        static_cast<uint32_t>(this->extensions.size());
    create_info.ppEnabledExtensionNames = this->extensions.data();
    VkDevice device;
    CheckVkResult(
        vkCreateDevice(physicalDevice, &create_info, nullptr, &device));

    reset(device, queueFamily);

    return VK_SUCCESS;
  }

  void reset(VkDevice _device, uint32_t _quemeFamily) {
    assert(this->device == VK_NULL_HANDLE);
    this->device = _device;
    this->queueFamily = _quemeFamily;
    vkGetDeviceQueue(this->device, this->queueFamily, 0, &this->queue);
  }

  VkResult submit(VkCommandBuffer cmd, VkSemaphore acquireSemaphore,
                  VkSemaphore submitSemaphore, VkFence submitFence) const {
    VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &acquireSemaphore,
        .pWaitDstStageMask = &waitDstStageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &submitSemaphore,
    };
    return vkQueueSubmit(this->queue, 1, &submitInfo, submitFence);
  }
};

struct Swapchain : public NonCopyable {
  VkInstance instance;
  VkSurfaceKHR surface;
  VkPhysicalDevice physicalDevice;
  uint32_t presentFamily;
  VkDevice device;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> images;

  std::vector<VkSurfaceFormatKHR> formats;
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  VkQueue presentQueue;
  std::vector<VkPresentModeKHR> presentModes;

  Swapchain(VkInstance _instance, VkSurfaceKHR _surface,
            VkPhysicalDevice _physicalDevice, uint32_t _presentFamily,
            VkDevice _device)
      : instance(_instance), surface(_surface), physicalDevice(_physicalDevice),
        presentFamily(_presentFamily), device(_device) {
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
    move(&rhs);
  }
  Swapchain &operator=(Swapchain &&rhs) {
    this->instance = rhs.instance;
    this->surface = rhs.surface;
    this->physicalDevice = rhs.physicalDevice;
    this->presentFamily = rhs.presentFamily;
    this->device = rhs.device;
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
    for (const auto &availableFormat : this->formats) {
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
          availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return availableFormat;
      }
      // switch (availableFormat.format) {
      // // Favor UNORM formats as the samples are not written for sRGB
      // currently. case VK_FORMAT_R8G8B8A8_UNORM: case
      // VK_FORMAT_B8G8R8A8_UNORM: case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
      //   return availableFormat;
      // default:
      //   break;
      // }
    }
    return this->formats[0];
  }
  VkPresentModeKHR chooseSwapPresentMode() const {
#ifdef ANDROID
#else
    for (const auto &availablePresentMode : this->presentModes) {
      if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return availablePresentMode;
      }
    }
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

  VkResult create(uint32_t graphicsFamily,
                  VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE) {
    auto surfaceFormat = chooseSwapSurfaceFormat();

    this->createInfo.surface = surface;
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
    this->createInfo.minImageCount =
        this->surfaceCapabilities.minImageCount + 1;
    this->createInfo.imageFormat = surfaceFormat.format;
    this->createInfo.imageColorSpace = surfaceFormat.colorSpace;
    this->createInfo.imageExtent = this->surfaceCapabilities.currentExtent;
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
    this->createInfo.preTransform = this->surfaceCapabilities.currentTransform;
    this->createInfo.presentMode = chooseSwapPresentMode();
    this->createInfo.oldSwapchain = oldSwapchain;
    auto result = vkCreateSwapchainKHR(this->device, &this->createInfo, nullptr,
                                       &this->swapchain);
    if (result != VK_SUCCESS) {
      return result;
    }

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(this->device, this->swapchain, &imageCount,
                            nullptr);
    Logger::Info("swapchain images: %d\n", imageCount);
    if (imageCount > 0) {
      this->images.resize(imageCount);
      vkGetSwapchainImagesKHR(this->device, this->swapchain, &imageCount,
                              this->images.data());
    }

    return VK_SUCCESS;
  }

  struct AcquiredImage {
    VkResult result;
    int64_t presentTimeNano;
    uint32_t imageIndex;
    VkImage image;
  };

  AcquiredImage acquireNextImage(VkSemaphore imageAvailableSemaphore) {
    uint32_t imageIndex;
    auto result = vkAcquireNextImageKHR(this->device, this->swapchain,
                                        UINT64_MAX, imageAvailableSemaphore,
                                        VK_NULL_HANDLE, &imageIndex);
    if (result != VK_SUCCESS) {
      return {result};
    }

    auto now = std::chrono::system_clock::now();
    auto epoch_time = now.time_since_epoch();
    auto epoch_time_nano =
        std::chrono::duration_cast<std::chrono::nanoseconds>(epoch_time)
            .count();

    return {
        result,
        epoch_time_nano,
        imageIndex,
        this->images[imageIndex],
    };
  }

  VkResult present(uint32_t imageIndex, VkSemaphore submitSemaphore) {
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &submitSemaphore,
        // swapchain
        .swapchainCount = 1,
        .pSwapchains = &this->swapchain,
        .pImageIndices = &imageIndex,
    };
    return vkQueuePresentKHR(this->presentQueue, &presentInfo);
  }
};

} // namespace vk
} // namespace vuloxr

#include <climits>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <list>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#ifdef ANDROID
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "MaliSDK", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "MaliSDK", __VA_ARGS__)
#else
#define LOGE(...) fprintf(stderr, "ERROR: " __VA_ARGS__)
#define LOGI(...) fprintf(stderr, "INFO: " __VA_ARGS__)
#endif

/// @brief Helper macro to test the result of Vulkan calls which can return an
/// error.
#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
      LOGE("Detected Vulkan error %d at %s:%d.\n", int(err), __FILE__,         \
           __LINE__);                                                          \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#define ASSERT_VK_HANDLE(handle)                                               \
  do {                                                                         \
    if ((handle) == VK_NULL_HANDLE) {                                          \
      LOGE("Handle is NULL at %s:%d.\n", __FILE__, __LINE__);                  \
      abort();                                                                 \
    }                                                                          \
  } while (0)

// vk object sunawati vko
namespace vko {

struct not_copyable {
  not_copyable() = default;
  ~not_copyable() = default;
  not_copyable(const not_copyable &) = delete;
  not_copyable &operator=(const not_copyable &) = delete;
};

// https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Physical_devices_and_queue_families
struct PhysicalDevice : public not_copyable {
  VkPhysicalDevice _physicaldevice;
  VkPhysicalDeviceProperties _deviceProperties;
  VkPhysicalDeviceFeatures _deviceFeatures;
  std::vector<VkQueueFamilyProperties> _queueFamilies;
  uint32_t _graphicsFamily = UINT_MAX;
  PhysicalDevice(VkPhysicalDevice physicalDevice)
      : _physicaldevice(physicalDevice) {
    vkGetPhysicalDeviceProperties(_physicaldevice, &_deviceProperties);
    vkGetPhysicalDeviceFeatures(_physicaldevice, &_deviceFeatures);
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                             nullptr);
    _queueFamilies.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                             _queueFamilies.data());

    for (uint32_t i = 0; i < _queueFamilies.size(); ++i) {
      if (_queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        _graphicsFamily = i;
        break;
      }
    }
  }
  PhysicalDevice(PhysicalDevice &&rhs) {
    this->_physicaldevice = rhs._physicaldevice;
    rhs._physicaldevice = {};
    this->_deviceProperties = rhs._deviceProperties;
    rhs._deviceProperties = {};
    this->_deviceFeatures = rhs._deviceFeatures;
    rhs._deviceFeatures = {};
    this->_queueFamilies = rhs._queueFamilies;
    rhs._queueFamilies = {};
    this->_graphicsFamily = rhs._graphicsFamily;
    rhs._graphicsFamily = UINT_MAX;
  }
  PhysicalDevice &operator=(PhysicalDevice &&rhs) {
    this->_physicaldevice = std::move(rhs._physicaldevice);
    rhs._physicaldevice = {};
    this->_deviceProperties = rhs._deviceProperties;
    rhs._deviceProperties = {};
    this->_deviceFeatures = rhs._deviceFeatures;
    rhs._deviceFeatures = {};
    this->_queueFamilies = rhs._queueFamilies;
    rhs._queueFamilies = {};
    this->_graphicsFamily = rhs._graphicsFamily;
    rhs._graphicsFamily = UINT_MAX;
    return *this;
  }
  std::optional<uint32_t> getPresentQueueFamily(VkSurfaceKHR surface) {
    for (uint32_t i = 0; i < _queueFamilies.size(); ++i) {
      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(_physicaldevice, i, surface,
                                           &presentSupport);
      if (presentSupport) {
        return i;
      }
    }
    return {};
  }
};

// https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers
struct Instance : public not_copyable {
  VkInstance _instance = VK_NULL_HANDLE;
  operator VkInstance() { return _instance; }
  ~Instance() {
    if (_debugUtilsMessenger != VK_NULL_HANDLE) {
      DestroyDebugUtilsMessengerEXT(_instance, _debugUtilsMessenger, nullptr);
    }
    if (_instance != VK_NULL_HANDLE) {
      vkDestroyInstance(_instance, nullptr);
    }
  }

  std::vector<PhysicalDevice> _devices;

  std::vector<const char *> _validationLayers;
  std::vector<const char *> _instanceExtensions;

  // VK_EXT_DEBUG_UTILS_EXTENSION_NAME
  VkDebugUtilsMessengerEXT _debugUtilsMessenger = VK_NULL_HANDLE;
  VkDebugUtilsMessengerCreateInfoEXT _debug_messenger_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback =
          [](VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
             VkDebugUtilsMessageTypeFlagsEXT message_types,
             const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
             void *user_data) {
            if (message_severity >=
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
              LOGE("%s %s\n", callback_data->pMessageIdName,
                   callback_data->pMessage);
            }
            return VK_FALSE;
          },
  };

  static VkResult CreateDebugUtilsMessengerEXT(
      VkInstance instance,
      const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
      const VkAllocationCallbacks *pAllocator,
      VkDebugUtilsMessengerEXT *pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
      return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
  }

  static void
  DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                VkDebugUtilsMessengerEXT debugMessenger,
                                const VkAllocationCallbacks *pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
      func(instance, debugMessenger, pAllocator);
    }
  }

  VkApplicationInfo _appInfo{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "VKO",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };

  VkInstanceCreateInfo _createInfo{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = nullptr,
      .pApplicationInfo = &_appInfo,
      .enabledLayerCount = 0,
      .enabledExtensionCount = 0,
  };

  VkResult create() {
    if (_validationLayers.size() > 0) {
      for (auto name : _validationLayers) {
        LOGI("instance layer: %s\n", name);
      }
      _createInfo.enabledLayerCount = _validationLayers.size();
      _createInfo.ppEnabledLayerNames = _validationLayers.data();
    }
    if (_instanceExtensions.size() > 0) {
      for (auto name : _instanceExtensions) {
        LOGI("instance extension: %s\n", name);
        if (strcmp(name, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
          _createInfo.pNext = &_debug_messenger_create_info;
        }
      }
      _createInfo.enabledExtensionCount = _instanceExtensions.size();
      _createInfo.ppEnabledExtensionNames = _instanceExtensions.data();
    }

    auto result = vkCreateInstance(&_createInfo, nullptr, &_instance);
    if (result != VK_SUCCESS) {
      return result;
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
      LOGE("no physical device\n");
    } else {
      std::vector<VkPhysicalDevice> devices(deviceCount);
      vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());
      for (auto d : devices) {
        _devices.push_back(PhysicalDevice(d));
      }
    }

    result =
        CreateDebugUtilsMessengerEXT(_instance, &_debug_messenger_create_info,
                                     nullptr, &_debugUtilsMessenger);
    return result;
  }

  struct SelectedPhysicalDevice {
    VkPhysicalDevice physicalDevice;
    uint32_t graphicsFamily;
    uint32_t presentFamily;
  };

  SelectedPhysicalDevice pickPhysicakDevice(VkSurfaceKHR surface) {
    SelectedPhysicalDevice ret{
        .physicalDevice = VK_NULL_HANDLE,
        .graphicsFamily = UINT_MAX,
        .presentFamily = UINT_MAX,
    };
    for (auto &d : _devices) {
      auto _presentFamily = d.getPresentQueueFamily(surface);
      if (d._deviceProperties.deviceType ==
              VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
          d._deviceFeatures.geometryShader && d._graphicsFamily != UINT_MAX &&
          _presentFamily) {
        auto selected = ret.physicalDevice == VK_NULL_HANDLE;
        if (selected) {
          ret.physicalDevice = d._physicaldevice;
          ret.graphicsFamily = d._graphicsFamily;
          ret.presentFamily = _presentFamily.value();
        }
        LOGI("physical device: %s: graphics => %d, present => %d, %s\n",
             d._deviceProperties.deviceName, d._graphicsFamily,
             _presentFamily.value(), (selected ? "select" : ""));
      } else {
        LOGE("physical device: %s: not supported\n",
             d._deviceProperties.deviceName);
      }
    }
    return ret;
  }
};

struct Device : public not_copyable {
  VkDevice _device = VK_NULL_HANDLE;
  operator VkDevice() { return _device; }
  ~Device() {
    if (_device != VK_NULL_HANDLE) {
      vkDestroyDevice(_device, nullptr);
    }
  }
  std::vector<const char *> _validationLayers;
  std::vector<const char *> _deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };
  float _queuePriority = 1.0f;
  std::vector<VkDeviceQueueCreateInfo> _queueCreateInfos;
  VkPhysicalDeviceFeatures _deviceFeatures{};
  VkDeviceCreateInfo _createInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 0,
      .enabledLayerCount = 0,
      .enabledExtensionCount = 0,
      .pEnabledFeatures = &_deviceFeatures,
  };

  VkResult create(VkPhysicalDevice physicalDevice, uint32_t graphics,
                  uint32_t present) {
    if (_validationLayers.size() > 0) {
      for (auto name : _validationLayers) {
        LOGI("device layer: %s\n", name);
      }
      _createInfo.enabledLayerCount = _validationLayers.size();
      _createInfo.ppEnabledLayerNames = _validationLayers.data();
    }
    if (_deviceExtensions.size() > 0) {
      for (auto name : _deviceExtensions) {
        LOGI("device extension: %s\n", name);
      }
      _createInfo.enabledExtensionCount = _deviceExtensions.size();
      _createInfo.ppEnabledExtensionNames = _deviceExtensions.data();
    }

    std::set<uint32_t> uniqueQueueFamilies = {graphics, present};
    for (uint32_t queueFamily : uniqueQueueFamilies) {
      VkDeviceQueueCreateInfo queueCreateInfo{
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = queueFamily,
          .queueCount = 1,
          .pQueuePriorities = &_queuePriority,
      };
      _queueCreateInfos.push_back(queueCreateInfo);
    }
    _createInfo.queueCreateInfoCount =
        static_cast<uint32_t>(_queueCreateInfos.size());
    _createInfo.pQueueCreateInfos = _queueCreateInfos.data();

    return vkCreateDevice(physicalDevice, &_createInfo, nullptr, &_device);
  }
};

struct Fence : public not_copyable {
  VkDevice _device;
  VkFence _fence = VK_NULL_HANDLE;
  operator VkFence() { return _fence; }
  Fence(VkDevice device, bool signaled) : _device(device) {
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    if (signaled) {
      fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }
    VK_CHECK(vkCreateFence(_device, &fenceInfo, nullptr, &_fence));
  }
  ~Fence() {
    if (_fence != VK_NULL_HANDLE) {
      vkDestroyFence(_device, _fence, nullptr);
    }
  }
  void reset() { vkResetFences(_device, 1, &_fence); }

  void block() {
    VK_CHECK(vkWaitForFences(_device, 1, &_fence, VK_TRUE, UINT64_MAX));
  }
};

struct Semaphore : public not_copyable {
  VkDevice _device;
  VkSemaphore _semaphore = VK_NULL_HANDLE;
  operator VkSemaphore() { return _semaphore; }
  Semaphore(VkDevice device) : _device(device) {
    VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    VK_CHECK(
        vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_semaphore));
  }
  ~Semaphore() { vkDestroySemaphore(_device, _semaphore, nullptr); }
};

struct Surface : public not_copyable {
  VkInstance _instance;
  VkSurfaceKHR _surface;
  VkPhysicalDevice _physicalDevice;
  VkSurfaceCapabilitiesKHR _capabilities;
  std::vector<VkSurfaceFormatKHR> _formats;
  std::vector<VkPresentModeKHR> _presentModes;

  Surface(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice gpu)
      : _instance(instance), _surface(surface), _physicalDevice(gpu) {

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &_capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, nullptr);
    if (formatCount != 0) {
      _formats.resize(formatCount);
      vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount,
                                           _formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount,
                                              nullptr);
    if (presentModeCount != 0) {
      _presentModes.resize(presentModeCount);
      vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount,
                                                _presentModes.data());
    }
  }

  ~Surface() { vkDestroySurfaceKHR(_instance, _surface, nullptr); }

  VkSurfaceFormatKHR chooseSwapSurfaceFormat() const {
    for (const auto &availableFormat : _formats) {
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
          availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return availableFormat;
      }
    }
    return _formats[0];
  }

  VkPresentModeKHR chooseSwapPresentMode() const {
    for (const auto &availablePresentMode : _presentModes) {
      if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return availablePresentMode;
      }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
  }
};

struct Swapchain : public not_copyable {
  VkDevice _device;
  VkQueue _presentQueue = VK_NULL_HANDLE;
  VkSwapchainKHR _swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> _images;
  Swapchain(VkDevice device) : _device(device) {}

  std::list<std::shared_ptr<Semaphore>> _imageAvailableSemaphorePool;
  std::vector<std::shared_ptr<Semaphore>> _submitCompleteSemaphores;

  ~Swapchain() {
    if (_swapchain != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    }
  }

  uint32_t _queueFamilyIndices[2] = {};
  VkSwapchainCreateInfoKHR _createInfo{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .imageArrayLayers = 1,
      .imageUsage =
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .clipped = VK_TRUE,
  };

  VkResult create(VkPhysicalDevice gpu, VkSurfaceKHR surface,
                  VkSurfaceFormatKHR surfaceFormat,
                  VkPresentModeKHR presentMode, uint32_t graphicsFamily,
                  uint32_t presentFamily,
                  VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE) {
    vkGetDeviceQueue(_device, presentFamily, 0, &_presentQueue);

    // get here for latest currentExtent
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &capabilities);

    _createInfo.surface = surface;
    _createInfo.minImageCount = capabilities.minImageCount + 1;
    _createInfo.imageFormat = surfaceFormat.format;
    _createInfo.imageColorSpace = surfaceFormat.colorSpace;
    _createInfo.imageExtent = capabilities.currentExtent;
    if (graphicsFamily == presentFamily) {
      _createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      _createInfo.queueFamilyIndexCount = 0;     // Optional
      _createInfo.pQueueFamilyIndices = nullptr; // Optional
    } else {
      _createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      _createInfo.queueFamilyIndexCount = 2;
      _queueFamilyIndices[0] = graphicsFamily;
      _queueFamilyIndices[1] = presentFamily;
      _createInfo.pQueueFamilyIndices = _queueFamilyIndices;
    }
    _createInfo.preTransform = capabilities.currentTransform;
    _createInfo.presentMode = presentMode;
    _createInfo.oldSwapchain = oldSwapchain;
    auto result =
        vkCreateSwapchainKHR(_device, &_createInfo, nullptr, &_swapchain);
    if (result != VK_SUCCESS) {
      return result;
    }

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(_device, _swapchain, &imageCount, nullptr);
    LOGI("swapchain images: %d\n", imageCount);
    if (imageCount > 0) {
      _images.resize(imageCount);
      vkGetSwapchainImagesKHR(_device, _swapchain, &imageCount, _images.data());
    }

    _submitCompleteSemaphores.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
      _submitCompleteSemaphores[i] = std::make_shared<Semaphore>(_device);
    }

    return VK_SUCCESS;
  }

  std::shared_ptr<Semaphore> getOrCreateImageAvailableSemaphore() {
    if (!_imageAvailableSemaphorePool.empty()) {
      auto s = _imageAvailableSemaphorePool.front();
      _imageAvailableSemaphorePool.pop_front();
      return s;
    }

    LOGI("new semaphore\n");
    auto s = std::make_shared<Semaphore>(_device);
    return s;
  }

  std::tuple<VkResult, uint32_t, VkImage, std::shared_ptr<Semaphore>,
             std::shared_ptr<Semaphore>>
  acquireNextImage() {
    auto imageAvailableSemaphore = getOrCreateImageAvailableSemaphore();

    uint32_t imageIndex;
    auto result = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX,
                                        *imageAvailableSemaphore,
                                        VK_NULL_HANDLE, &imageIndex);
    return {result, imageIndex, _images[imageIndex], imageAvailableSemaphore,
            _submitCompleteSemaphores[imageIndex]};
  }

  VkResult present(uint32_t imageIndex,
                   const std::shared_ptr<Semaphore> &imageAvailableSemaphore) {
    _imageAvailableSemaphorePool.push_back(imageAvailableSemaphore);

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &_submitCompleteSemaphores[imageIndex]->_semaphore,
        // swapchain
        .swapchainCount = 1,
        .pSwapchains = &_swapchain,
        .pImageIndices = &imageIndex,
    };
    return vkQueuePresentKHR(_presentQueue, &presentInfo);
  }
};

} // namespace vko

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

  VkCommandBuffer getRecordedCommand(VkImage image,
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

    VkClearColorValue clearColorValue = {1.0, 0.0, 0.0, 0.0};
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

      auto [result, imageIndex, image, imageAvailableSemaphore,
            submitCompleteSemaphore] = swapchain.acquireNextImage();
      VK_CHECK(result);

      auto commandBuffer =
          renderer.getRecordedCommand(image, picked.graphicsFamily);

      VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
      VkSubmitInfo submitInfo = {
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = &imageAvailableSemaphore->_semaphore,
          .pWaitDstStageMask = &waitDstStageMask,
          .commandBufferCount = 1,
          .pCommandBuffers = &commandBuffer,
          .signalSemaphoreCount = 1,
          .pSignalSemaphores = &submitCompleteSemaphore->_semaphore,
      };
      submitCompleteFence.reset();
      VK_CHECK(
          vkQueueSubmit(graphicsQueue, 1, &submitInfo, submitCompleteFence));

      submitCompleteFence.block();
      VK_CHECK(swapchain.present(imageIndex, imageAvailableSemaphore));
    }

    vkDeviceWaitIdle(device);
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

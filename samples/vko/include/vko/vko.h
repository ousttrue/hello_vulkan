/// vk object sunawati vko
///
/// Type name: UpperCamel
/// Field name: lowerCamel
/// Method name: lowerCamel
///
#pragma once
#include <chrono>
#include <climits>
#include <list>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

/// @brief Helper macro to test the result of Vulkan calls which can return an
/// error.
#ifndef VKO_CHECK
#define VKO_CHECK(x)                                                           \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
      vko::Logger::Error("Detected Vulkan error %d at %s:%d.\n", int(err),     \
                         __FILE__, __LINE__);                                  \
      abort();                                                                 \
    }                                                                          \
  } while (0)
#endif

#ifdef ANDROID
#include <android/log.h>
#else
#include <stdarg.h>
#endif

inline PFN_vkSetDebugUtilsObjectNameEXT
g_vkSetDebugUtilsObjectNameEXT(VkInstance instance = VK_NULL_HANDLE) {
  static PFN_vkSetDebugUtilsObjectNameEXT s;
  if (instance != VK_NULL_HANDLE) {
    s = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(
        instance, "vkSetDebugUtilsObjectNameEXT");
  }
  return s;
}

inline VkResult SetDebugUtilsObjectNameEXT(VkDevice device,
                                           VkObjectType objectType,
                                           uint64_t objectHandle,
                                           const char *pObjectName) {
  VkDebugUtilsObjectNameInfoEXT nameInfo{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .pNext = nullptr,
      .objectType = objectType,
      .objectHandle = objectHandle,
      .pObjectName = pObjectName,
  };
  return g_vkSetDebugUtilsObjectNameEXT()(device, &nameInfo);
}

namespace vko {

struct Logger {
#ifdef ANDROID
  static void Info(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, "vko", fmt, arg);
    va_end(arg);
  }
  static void Error(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    __android_log_vprint(ANDROID_LOG_ERROR, "vko", fmt, arg);
    va_end(arg);
  }
#else
  static void Info(const char *_fmt, ...) {
    va_list arg;
    va_start(arg, _fmt);
    auto fmt = (std::string("INFO: ") + _fmt);
    if (!fmt.ends_with('\n')) {
      fmt += '\n';
    }
    vfprintf(stderr, fmt.c_str(), arg);
    va_end(arg);
  }
  static void Error(const char *_fmt, ...) {
    va_list arg;
    va_start(arg, _fmt);
    auto fmt = (std::string("ERROR: ") + _fmt);
    if (!fmt.ends_with('\n')) {
      fmt += '\n';
    }
    vfprintf(stderr, fmt.c_str(), arg);
    va_end(arg);
  }
#endif
};

struct not_copyable {
  not_copyable() = default;
  ~not_copyable() = default;
  not_copyable(const not_copyable &) = delete;
  not_copyable &operator=(const not_copyable &) = delete;
};

// https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Physical_devices_and_queue_families
struct PhysicalDevice {
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  operator VkPhysicalDevice() const { return this->physicalDevice; }
  VkPhysicalDeviceProperties properties = {};
  VkPhysicalDeviceFeatures features = {};
  std::vector<VkQueueFamilyProperties> queueFamilyProperties;
  uint32_t graphicsFamilyIndex = UINT_MAX;
  uint32_t presentFamilyIndex = UINT_MAX;

  PhysicalDevice() {}

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
  }

  std::optional<uint32_t> getPresentQueueFamily(VkSurfaceKHR surface) {
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

  static const char *deviceTypeStr(VkPhysicalDeviceType t) {
    switch (t) {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
      return "OTHER";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      return "INTEGRATED_GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      return "DISCRETE_GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      return "VIRTUAL_GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      return "CPU";
    default:
      return "unknown";
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

  void debugPrint(VkSurfaceKHR surface) {
    Logger::Info("[%s] %s", this->properties.deviceName,
                 deviceTypeStr(this->properties.deviceType));
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

  std::optional<uint32_t> isSuitable(VkSurfaceKHR surface) {
    auto presentFamily = getPresentQueueFamily(surface);
    if (presentFamily) {
      this->presentFamilyIndex = *presentFamily;
    }
    if (this->graphicsFamilyIndex != UINT_MAX &&
        this->presentFamilyIndex != UINT_MAX) {
      return presentFamily;
    }
    return {};
  }
};

inline const std::vector<VkLayerProperties> &availableLayers() {
  static std::vector<VkLayerProperties> s_availableLayers;
  if (s_availableLayers.empty()) {
    uint32_t layerCount;
    VKO_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));
    s_availableLayers.resize(layerCount);
    VKO_CHECK(vkEnumerateInstanceLayerProperties(&layerCount,
                                                 s_availableLayers.data()));
  }
  return s_availableLayers;
}

inline bool layerIsSupported(const char *name) {
  for (const auto &layerProperties : availableLayers()) {
    if (0 == strcmp(name, layerProperties.layerName)) {
      return true;
    }
  }
  return false;
}

inline const std::vector<VkExtensionProperties> availableInstanceExtensions() {
  static std::vector<VkExtensionProperties> s_availableExtensions;
  if (s_availableExtensions.empty()) {
    uint32_t extensionCount = 0;
    VKO_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                                     nullptr));
    s_availableExtensions.resize(extensionCount);
    VKO_CHECK(vkEnumerateInstanceExtensionProperties(
        nullptr, &extensionCount, s_availableExtensions.data()));
  }
  return s_availableExtensions;
}

inline bool instanceExtensionIsSupported(const char *name) {
  for (const auto &properties : availableInstanceExtensions()) {
    if (0 == strcmp(name, properties.extensionName)) {
      return true;
    }
  }
  return false;
}

// https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers
struct Instance : public not_copyable {
  VkInstance instance = VK_NULL_HANDLE;

  operator VkInstance() const { return this->instance; }
  ~Instance() {
    if (this->debugUtilsMessenger != VK_NULL_HANDLE) {
      DestroyDebugUtilsMessengerEXT(this->instance, this->debugUtilsMessenger,
                                    nullptr);
    }
    if (this->instance != VK_NULL_HANDLE) {
      vkDestroyInstance(this->instance, nullptr);
    }
  }

  void setInstance(VkInstance _instance) {
    this->instance = _instance;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(this->instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
      Logger::Error("no physical device\n");
    } else {
      std::vector<VkPhysicalDevice> devices(deviceCount);
      vkEnumeratePhysicalDevices(this->instance, &deviceCount, devices.data());
      for (auto d : devices) {
        this->physicalDevices.push_back(PhysicalDevice(d));
      }
    }

    VKO_CHECK( CreateDebugUtilsMessengerEXT(this->instance,
                                          &this->debug_messenger_create_info,
                                          nullptr, &this->debugUtilsMessenger));

    g_vkSetDebugUtilsObjectNameEXT(this->instance);
  }

  std::vector<PhysicalDevice> physicalDevices;

  std::vector<const char *> validationLayers;
  std::vector<const char *> instanceExtensions;

  // VK_EXT_DEBUG_UTILS_EXTENSION_NAME
  VkDebugUtilsMessengerEXT debugUtilsMessenger = VK_NULL_HANDLE;
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
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
              Logger::Error("%s %s\n", callback_data->pMessageIdName,
                            callback_data->pMessage);
            }
            return VK_FALSE;
          },
  };
  // VkDebugUtilsMessengerCreateInfoEXT debugInfo{
  //     .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
  //     .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
  //                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
  //     .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
  //                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
  //                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
  //     .pfnUserCallback = debugMessageThunk,
  // };

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

  VkApplicationInfo appInfo{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "VKO",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
      // .apiVersion = VK_MAKE_VERSION(1, 0, 24),
  };

  VkInstanceCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = nullptr,
      .pApplicationInfo = &this->appInfo,
      .enabledLayerCount = 0,
      .enabledExtensionCount = 0,
  };

  VkResult create() {
    if (this->validationLayers.size() > 0) {
      for (auto name : this->validationLayers) {
        Logger::Info("instance layer: %s\n", name);
      }
      this->createInfo.enabledLayerCount = this->validationLayers.size();
      this->createInfo.ppEnabledLayerNames = this->validationLayers.data();
    }
    if (this->instanceExtensions.size() > 0) {
      for (auto name : this->instanceExtensions) {
        Logger::Info("instance extension: %s\n", name);
        if (strcmp(name, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
          this->createInfo.pNext = &this->debug_messenger_create_info;
        }
      }
      this->createInfo.enabledExtensionCount = this->instanceExtensions.size();
      this->createInfo.ppEnabledExtensionNames =
          this->instanceExtensions.data();
    }

    VkInstance instance;
    auto result = vkCreateInstance(&this->createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
      // Try to fall back to compatible Vulkan versions if the driver is using
      // older, but compatible API versions.
      // if (res == VK_ERROR_INCOMPATIBLE_DRIVER) {
      //   app.apiVersion = VK_MAKE_VERSION(1, 0, 1);
      //   res = vkCreateInstance(&instanceInfo, nullptr, &instance);
      //   if (res == VK_SUCCESS) {
      //     Logger::Info("Created Vulkan instance with API
      //     version 1.0.1.\n");
      //   }
      // }
      // if (res == VK_ERROR_INCOMPATIBLE_DRIVER) {
      //   app.apiVersion = VK_MAKE_VERSION(1, 0, 2);
      //   res = vkCreateInstance(&instanceInfo, nullptr, &instance);
      //   if (res == VK_SUCCESS)
      //     Logger::Info("Created Vulkan instance with API
      //     version 1.0.2.\n");
      // }
      // if (res != VK_SUCCESS) {
      //   Logger::Error("Failed to create Vulkan instance (error: %d).\n",
      //   int(res)); return {};
      // }
      return result;
    }

    setInstance(instance);

    return result;
  }

  PhysicalDevice pickPhysicalDevice(VkSurfaceKHR surface) {
    PhysicalDevice picked;
    for (auto &physicalDevice : this->physicalDevices) {
      physicalDevice.debugPrint(surface);
      if (auto presentFamily = physicalDevice.isSuitable(surface)) {
        if (picked.physicalDevice == VK_NULL_HANDLE) {
          // use 1st
          picked = physicalDevice;
        }
      }
    }
    if (this->physicalDevices.size() > 0) {
      // fall back. use 1st device
      picked = this->physicalDevices[0];
    }
    return picked;
  }
};

struct Device : public not_copyable {
  VkDevice device = VK_NULL_HANDLE;
  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkQueue presentQueue = VK_NULL_HANDLE;
  operator VkDevice() const { return this->device; }
  ~Device() {
    if (this->device != VK_NULL_HANDLE) {
      vkDestroyDevice(this->device, nullptr);
    }
  }
  void setDevice(VkDevice _device) { this->device = _device; }
  std::vector<const char *> validationLayers;
  std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };
  std::vector<float> queuePriorities = {1.0f};
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  VkPhysicalDeviceFeatures deviceFeatures{
      .samplerAnisotropy = VK_TRUE,
  };
  VkDeviceCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 0,
      .enabledLayerCount = 0,
      .enabledExtensionCount = 0,
      .pEnabledFeatures = &this->deviceFeatures,
  };

  VkResult create(VkPhysicalDevice physicalDevice, uint32_t graphics,
                  uint32_t present) {
    if (this->validationLayers.size() > 0) {
      for (auto name : this->validationLayers) {
        Logger::Info("device layer: %s\n", name);
      }
      this->createInfo.enabledLayerCount = this->validationLayers.size();
      this->createInfo.ppEnabledLayerNames = this->validationLayers.data();
    }
    if (this->deviceExtensions.size() > 0) {
      for (auto name : this->deviceExtensions) {
        Logger::Info("device extension: %s\n", name);
      }
      this->createInfo.enabledExtensionCount = this->deviceExtensions.size();
      this->createInfo.ppEnabledExtensionNames = this->deviceExtensions.data();
    }

    std::set<uint32_t> uniqueQueueFamilies = {graphics, present};
    for (uint32_t queueFamily : uniqueQueueFamilies) {
      VkDeviceQueueCreateInfo queueCreateInfo{
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .pNext = 0,
          .flags = 0,
          .queueFamilyIndex = queueFamily,
          .queueCount = static_cast<uint32_t>(this->queuePriorities.size()),
          .pQueuePriorities = this->queuePriorities.data(),
      };
      this->queueCreateInfos.push_back(queueCreateInfo);
    }
    this->createInfo.queueCreateInfoCount =
        static_cast<uint32_t>(this->queueCreateInfos.size());
    this->createInfo.pQueueCreateInfos = this->queueCreateInfos.data();

    auto result = vkCreateDevice(physicalDevice, &this->createInfo, nullptr,
                                 &this->device);
    if (result != VK_SUCCESS) {
      return result;
    }

    vkGetDeviceQueue(this->device, graphics, 0, &this->graphicsQueue);
    vkGetDeviceQueue(this->device, present, 0, &this->presentQueue);

    return VK_SUCCESS;
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
    return vkQueueSubmit(this->graphicsQueue, 1, &submitInfo, submitFence);
  }
};

struct Fence : public not_copyable {
  VkDevice device;
  VkFence fence = VK_NULL_HANDLE;
  operator VkFence() const { return this->fence; }
  Fence(VkDevice _device, bool signaled) : device(_device) {
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    if (signaled) {
      fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }
    VKO_CHECK(vkCreateFence(this->device, &fenceInfo, nullptr, &this->fence));
  }
  ~Fence() {
    if (this->fence != VK_NULL_HANDLE) {
      vkDestroyFence(this->device, this->fence, nullptr);
    }
  }
  void reset() { vkResetFences(this->device, 1, &this->fence); }

  void block() {
    VKO_CHECK(
        vkWaitForFences(this->device, 1, &this->fence, VK_TRUE, UINT64_MAX));
  }
};

struct Semaphore : public not_copyable {
  VkDevice device;
  VkSemaphore semaphore = VK_NULL_HANDLE;
  operator VkSemaphore() const { return this->semaphore; }
  Semaphore(VkDevice _device) : device(_device) {
    VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    VKO_CHECK(vkCreateSemaphore(this->device, &semaphoreCreateInfo, nullptr,
                                &this->semaphore));
  }
  ~Semaphore() { vkDestroySemaphore(this->device, this->semaphore, nullptr); }
};

class SemaphorePool {
  VkDevice device = VK_NULL_HANDLE;
  std::list<VkSemaphore> owned;
  std::list<VkSemaphore> semaphores;

public:
  SemaphorePool(VkDevice _device) : device(_device) {}
  ~SemaphorePool() {
    for (auto &semaphore : this->owned) {
      vkDestroySemaphore(this->device, semaphore, nullptr);
    }
  }
  VkSemaphore getOrCreateSemaphore() {
    if (!this->semaphores.empty()) {
      auto semaphore = this->semaphores.front();
      this->semaphores.pop_front();
      return semaphore;
    }
    VkSemaphoreCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkSemaphore semaphore;
    VKO_CHECK(vkCreateSemaphore(this->device, &info, nullptr, &semaphore));
    this->owned.push_back(semaphore);
    return semaphore;
  }
  void returnSemaphore(VkSemaphore semaphore) {
    this->semaphores.push_back(semaphore);
  }
};

struct Surface : public not_copyable {
  VkInstance instance;
  VkSurfaceKHR surface;
  operator VkSurfaceKHR() const { return this->surface; }
  VkPhysicalDevice physicalDevice;
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;

  Surface(VkInstance _instance, VkSurfaceKHR _surface,
          VkPhysicalDevice _physicalDevice)
      : instance(_instance), surface(_surface),
        physicalDevice(_physicalDevice) {

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        this->physicalDevice, this->surface, &this->capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(this->physicalDevice, this->surface,
                                         &formatCount, nullptr);
    if (formatCount != 0) {
      this->formats.resize(formatCount);
      vkGetPhysicalDeviceSurfaceFormatsKHR(this->physicalDevice, this->surface,
                                           &formatCount, this->formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        this->physicalDevice, this->surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
      this->presentModes.resize(presentModeCount);
      vkGetPhysicalDeviceSurfacePresentModesKHR(
          this->physicalDevice, this->surface, &presentModeCount,
          this->presentModes.data());
    }
  }

  ~Surface() { vkDestroySurfaceKHR(this->instance, this->surface, nullptr); }

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
};

struct Swapchain : public not_copyable {
  VkDevice device;
  VkQueue presentQueue = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> images;

  Swapchain(VkDevice _device) : device(_device) {}
  ~Swapchain() {
    if (this->swapchain != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(this->device, this->swapchain, nullptr);
    }
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

  VkResult create(VkPhysicalDevice gpu, VkSurfaceKHR surface,
                  VkSurfaceFormatKHR surfaceFormat,
                  VkPresentModeKHR presentMode, uint32_t graphicsFamily,
                  uint32_t presentFamily,
                  VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE) {
    vkGetDeviceQueue(this->device, presentFamily, 0, &this->presentQueue);

    // get here for latest currentExtent
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &capabilities);

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
    this->createInfo.minImageCount = capabilities.minImageCount + 1;
    this->createInfo.imageFormat = surfaceFormat.format;
    this->createInfo.imageColorSpace = surfaceFormat.colorSpace;
    this->createInfo.imageExtent = capabilities.currentExtent;
    if (graphicsFamily == presentFamily) {
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
    this->createInfo.preTransform = capabilities.currentTransform;
    this->createInfo.presentMode = presentMode;
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

struct SwapchainFramebuffer {
  VkDevice device;
  VkImageView imageView;
  VkFramebuffer framebuffer;

  VkImageViewCreateInfo _createInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
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
  VkFramebufferCreateInfo _framebufferInfo{
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .layers = 1,
  };

  SwapchainFramebuffer(VkDevice _device, VkImage image, VkExtent2D extent,
                       VkFormat format, VkRenderPass renderPass)
      : device(_device) {
    _createInfo.image = image;
    _createInfo.format = format;
    VKO_CHECK(
        vkCreateImageView(device, &_createInfo, nullptr, &this->imageView));

    _framebufferInfo.pAttachments = &this->imageView;
    _framebufferInfo.attachmentCount = 1;
    _framebufferInfo.renderPass = renderPass;
    _framebufferInfo.width = extent.width;
    _framebufferInfo.height = extent.height;
    VKO_CHECK(vkCreateFramebuffer(device, &_framebufferInfo, nullptr,
                                  &this->framebuffer));
  }

  ~SwapchainFramebuffer() {
    vkDestroyFramebuffer(this->device, this->framebuffer, nullptr);
    vkDestroyImageView(this->device, this->imageView, nullptr);
  }
};

struct Flight {
  VkFence submitFence;
  VkSemaphore submitSemaphore;
  // keep next frame
  VkSemaphore acquireSemaphore;
};

struct FlightManager {
  VkDevice device = VK_NULL_HANDLE;
  VkCommandPool pool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> commandBuffers;
  std::vector<Flight> flights;
  uint32_t frameCount = 0;

  std::list<VkSemaphore> acquireSemaphoresOwn;
  std::list<VkSemaphore> acquireSemaphoresReuse;

public:
  FlightManager(VkDevice _device, uint32_t graphicsQueueIndex,
                uint32_t flightCount)
      : device(_device), commandBuffers(flightCount), flights(flightCount) {
    Logger::Info("frames in flight: %d\n", flightCount);
    VkCommandPoolCreateInfo commandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsQueueIndex,
    };
    VKO_CHECK(vkCreateCommandPool(this->device, &commandPoolCreateInfo, nullptr,
                                  &this->pool));

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = this->pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = flightCount,
    };
    VKO_CHECK(vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo,
                                       this->commandBuffers.data()));

    for (auto &flight : this->flights) {
      VkFenceCreateInfo fenceInfo = {
          .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
          .flags = VK_FENCE_CREATE_SIGNALED_BIT,
      };
      VKO_CHECK(vkCreateFence(this->device, &fenceInfo, nullptr,
                              &flight.submitFence));

      VkSemaphoreCreateInfo semaphoreInfo = {
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      };
      VKO_CHECK(vkCreateSemaphore(this->device, &semaphoreInfo, nullptr,
                                  &flight.submitSemaphore));
    }
  }

  ~FlightManager() {
    for (auto semaphore : this->acquireSemaphoresOwn) {
      vkDestroySemaphore(this->device, semaphore, nullptr);
    }

    for (auto flight : this->flights) {
      vkDestroyFence(this->device, flight.submitFence, nullptr);
      vkDestroySemaphore(this->device, flight.submitSemaphore, nullptr);
    }

    if (this->commandBuffers.size()) {
      vkFreeCommandBuffers(this->device, this->pool,
                           this->commandBuffers.size(),
                           this->commandBuffers.data());
    }
    vkDestroyCommandPool(this->device, this->pool, nullptr);
  }

  VkSemaphore getOrCreateSemaphore() {
    if (!this->acquireSemaphoresReuse.empty()) {
      auto semaphroe = this->acquireSemaphoresReuse.front();
      this->acquireSemaphoresReuse.pop_front();
      return semaphroe;
    }

    vko::Logger::Info("* create acquireSemaphore *");
    VkSemaphore semaphore;
    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VKO_CHECK(
        vkCreateSemaphore(this->device, &semaphoreInfo, nullptr, &semaphore));
    this->acquireSemaphoresOwn.push_back(semaphore);
    return semaphore;
  }

  std::tuple<VkCommandBuffer, Flight, VkSemaphore>
  sync(VkSemaphore acquireSemaphore) {
    auto index = (this->frameCount++) % this->flights.size();
    // keep acquireSemaphore
    auto &flight = this->flights[index];
    auto oldSemaphore = flight.acquireSemaphore;
    flight.acquireSemaphore = acquireSemaphore;

    // block. ensure oldSemaphore be signaled.
    vkWaitForFences(this->device, 1, &flight.submitFence, true, UINT64_MAX);
    vkResetFences(this->device, 1, &flight.submitFence);

    auto cmd = this->commandBuffers[index];

    return {cmd, flight, oldSemaphore};
  }

  void reuseSemaphore(VkSemaphore semaphore) {
    this->acquireSemaphoresReuse.push_back(semaphore);
  }
};

} // namespace vko

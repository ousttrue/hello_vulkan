#pragma once
#include "../vuloxr.h"
#include <assert.h>
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
  std::vector<VkExtensionProperties> instanceExtensionProperties;

private:
  Platform() {
    // Enumerate available extensions
    uint32_t properties_count;
    vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
    this->instanceExtensionProperties.resize(properties_count);
    auto err = vkEnumerateInstanceExtensionProperties(
        nullptr, &properties_count, this->instanceExtensionProperties.data());
    CheckVkResult(err);
  }

public:
  static Platform &singleton() {
    static Platform s_platform;
    return s_platform;
  }

  bool isInstanceExtensionAvailable(const char *extension) {
    for (const VkExtensionProperties &p : this->instanceExtensionProperties)
      if (strcmp(p.extensionName, extension) == 0)
        return true;
    return false;
  }
};

struct PhysicalDevice : NonCopyable {
  VkPhysicalDevice physicalDevice;
  operator VkPhysicalDevice() const { return this->physicalDevice; }
  std::vector<VkExtensionProperties> deviceExtensionProperties;

  PhysicalDevice(VkPhysicalDevice _physicalDevice)
      : physicalDevice(_physicalDevice) {
    // Enumerate physical device extension
    uint32_t properties_count;
    vkEnumerateDeviceExtensionProperties(this->physicalDevice, nullptr,
                                         &properties_count, nullptr);
    this->deviceExtensionProperties.resize(properties_count);
    vkEnumerateDeviceExtensionProperties(
        this->physicalDevice, nullptr, &properties_count,
        this->deviceExtensionProperties.data());
  }

  bool isDeviceExtensionAvailable(const char *extension) {
    for (const VkExtensionProperties &p : this->deviceExtensionProperties)
      if (strcmp(p.extensionName, extension) == 0)
        return true;
    return false;
  }
};

struct Instance : NonCopyable {
  std::vector<const char *> layers = {"VK_LAYER_KHRONOS_validation"};
  std::vector<const char *> extensions;

  VkInstance instance = VK_NULL_HANDLE;
  operator VkInstance() const { return this->instance; }
  VkInstanceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
  };

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

  bool addExtension(const char *extension) {
    if (Platform::singleton().isInstanceExtensionAvailable(extension)) {
      this->extensions.push_back(extension);
      return true;
    } else {
      return false;
    }
  }

  ~Instance() {
    if (this->debugReport != VK_NULL_HANDLE) {
      // Remove the debug report callback
      auto f_vkDestroyDebugReportCallbackEXT =
          (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
              instance, "vkDestroyDebugReportCallbackEXT");
      f_vkDestroyDebugReportCallbackEXT(instance, this->debugReport, nullptr);
    }
    if (this->instance != VK_NULL_HANDLE) {
      vkDestroyInstance(instance, nullptr);
    }
  }

  VkResult create(bool useDebugReport) {
    // Enable required extensions
    addExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    if (useDebugReport) {
      addExtension("VK_EXT_debug_report");
    }

    create_info.enabledLayerCount =
        static_cast<uint32_t>(std::size(this->layers));
    create_info.ppEnabledLayerNames = this->layers.data();
    create_info.enabledExtensionCount =
        static_cast<uint32_t>(std::size(this->extensions));
    create_info.ppEnabledExtensionNames = this->extensions.data();

    auto err = vkCreateInstance(&this->create_info, nullptr /*g_Allocator*/,
                                &this->instance);
    CheckVkResult(err);

    // Setup the debug report callback
    if (useDebugReport) {
      auto f_vkCreateDebugReportCallbackEXT =
          (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
              this->instance, "vkCreateDebugReportCallbackEXT");
      assert(f_vkCreateDebugReportCallbackEXT);
      err = f_vkCreateDebugReportCallbackEXT(
          this->instance, &this->debug_report_ci, nullptr /*g_Allocator*/,
          &this->debugReport);
      CheckVkResult(err);
    }
    return VK_SUCCESS;
  }
};

struct Device : NonCopyable {
  std::vector<const char *> extensions = {
      "VK_KHR_swapchain",
  };
  VkDevice device = VK_NULL_HANDLE;
  operator VkDevice() const { return this->device; }
  ~Device() {
    if (this->device != VK_NULL_HANDLE) {
      vkDestroyDevice(this->device, nullptr);
    }
  }

  VkQueue queue = VK_NULL_HANDLE;

  // Create Logical Device (with 1 queue)
  VkResult create(VkInstance instance, const PhysicalDevice &physicalDevice,
                  uint32_t queueFamily) {
    const float queue_priority[] = {1.0f};
    VkDeviceQueueCreateInfo queue_info[1] = {};
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = queueFamily;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = queue_priority;
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount =
        sizeof(queue_info) / sizeof(queue_info[0]);
    create_info.pQueueCreateInfos = queue_info;
    create_info.enabledExtensionCount =
        static_cast<uint32_t>(this->extensions.size());
    create_info.ppEnabledExtensionNames = this->extensions.data();
    auto err =
        vkCreateDevice(physicalDevice, &create_info, nullptr, &this->device);

    CheckVkResult(err);
    vkGetDeviceQueue(device, queueFamily, 0, &this->queue);

    return err;
  }
};

} // namespace vk
} // namespace vuloxr

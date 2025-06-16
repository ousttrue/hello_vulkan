#pragma once
#include <vko/vko.h>
#ifdef XR_USE_PLATFORM_ANDROID
#include <android_native_app_glue.h>
#endif
#ifdef XR_USE_PLATFORM_WIN32
#include <Unknwn.h>
#endif
#include <openxr/openxr_platform.h>

#define XRO_CHECK(cmd) xro::CheckXrResult(cmd, #cmd, FILE_AND_LINE);
// #define CHECK_XRRESULT(res, cmdStr) CheckXrResult(res, cmdStr,
// FILE_AND_LINE);

namespace xro {

using Logger = vko::Logger;

[[noreturn]] inline void ThrowXrResult(XrResult res,
                                       const char *originator = nullptr,
                                       const char *sourceLocation = nullptr) {
  vko::Throw(vko::fmt("XrResult failure [%d]", res), originator,
             sourceLocation);
}

#define THROW_XR(xr, cmd) ThrowXrResult(xr, #cmd, FILE_AND_LINE);

inline XrResult CheckXrResult(XrResult res, const char *originator = nullptr,
                              const char *sourceLocation = nullptr) {
  if (XR_FAILED(res)) {
    ThrowXrResult(res, originator, sourceLocation);
  }

  return res;
}

struct Instance {
  XrInstance instance = XR_NULL_HANDLE;
  std::vector<const char *> extensions;
  XrInstanceCreateInfo createInfo{
      .type = XR_TYPE_INSTANCE_CREATE_INFO,
      // .next = next,
      .createFlags = 0,
      .applicationInfo =
          {.applicationName = "HelloXR",
           // Current version is 1.1.x, but hello_xr only requires 1.0.x
           .applicationVersion = {},
           .engineName = {},
           .engineVersion = {},
           .apiVersion = XR_API_VERSION_1_0},
      .enabledApiLayerCount = 0,
      .enabledApiLayerNames = nullptr,
      .enabledExtensionCount = 0,
  };

  XrSystemId systemId = XR_NULL_SYSTEM_ID;
  XrSystemGetInfo systemInfo{
      .type = XR_TYPE_SYSTEM_GET_INFO,
      .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
  };

  ~Instance() {
    if (this->instance != XR_NULL_HANDLE) {
      xrDestroyInstance(this->instance);
    }
  }

  XrResult create(void *next) {
    for (auto name : this->extensions) {
      Logger::Info("openxr extension: %s", name);
    }
    if (this->extensions.size()) {
      this->createInfo.enabledExtensionCount =
          static_cast<uint32_t>(this->extensions.size());
      this->createInfo.enabledExtensionNames = extensions.data();
    }
    this->createInfo.next = next;
    auto result = xrCreateInstance(&this->createInfo, &this->instance);
    if (result != XR_SUCCESS) {
      return result;
    }

    result = xrGetSystem(this->instance, &this->systemInfo, &this->systemId);
    if (result != XR_SUCCESS) {
      return result;
    }

    return XR_SUCCESS;
  }

  static XrResult GetVulkanGraphicsRequirements2KHR(
      XrInstance instance, XrSystemId systemId,
      XrGraphicsRequirementsVulkan2KHR *graphicsRequirements) {
    PFN_xrGetVulkanGraphicsRequirements2KHR
        pfnGetVulkanGraphicsRequirements2KHR = nullptr;
    if (xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirements2KHR",
                              reinterpret_cast<PFN_xrVoidFunction *>(
                                  &pfnGetVulkanGraphicsRequirements2KHR)) !=
        XR_SUCCESS) {
      throw std::runtime_error("xrGetInstanceProcAddr");
    }

    return pfnGetVulkanGraphicsRequirements2KHR(instance, systemId,
                                                graphicsRequirements);
  }

  static XrResult
  CreateVulkanInstanceKHR(XrInstance instance,
                          const XrVulkanInstanceCreateInfoKHR *createInfo,
                          VkInstance *vulkanInstance, VkResult *vulkanResult) {
    PFN_xrCreateVulkanInstanceKHR pfnCreateVulkanInstanceKHR = nullptr;
    if (xrGetInstanceProcAddr(instance, "xrCreateVulkanInstanceKHR",
                              reinterpret_cast<PFN_xrVoidFunction *>(
                                  &pfnCreateVulkanInstanceKHR)) != XR_SUCCESS) {
      throw std::runtime_error("xrGetInstanceProcAddr");
    }

    return pfnCreateVulkanInstanceKHR(instance, createInfo, vulkanInstance,
                                      vulkanResult);
  }

  static XrResult
  GetVulkanGraphicsDevice2KHR(XrInstance instance,
                              const XrVulkanGraphicsDeviceGetInfoKHR *getInfo,
                              VkPhysicalDevice *vulkanPhysicalDevice) {
    PFN_xrGetVulkanGraphicsDevice2KHR pfnGetVulkanGraphicsDevice2KHR = nullptr;
    if (xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDevice2KHR",
                              reinterpret_cast<PFN_xrVoidFunction *>(
                                  &pfnGetVulkanGraphicsDevice2KHR)) !=
        XR_SUCCESS) {
      throw std::runtime_error("xrGetInstanceProcAddr");
    }

    return pfnGetVulkanGraphicsDevice2KHR(instance, getInfo,
                                          vulkanPhysicalDevice);
  }

  static XrResult
  CreateVulkanDeviceKHR(XrInstance instance,
                        const XrVulkanDeviceCreateInfoKHR *createInfo,
                        VkDevice *vulkanDevice, VkResult *vulkanResult) {
    PFN_xrCreateVulkanDeviceKHR pfnCreateVulkanDeviceKHR = nullptr;
    if (xrGetInstanceProcAddr(instance, "xrCreateVulkanDeviceKHR",
                              reinterpret_cast<PFN_xrVoidFunction *>(
                                  &pfnCreateVulkanDeviceKHR)) != XR_SUCCESS) {
      throw std::runtime_error("xrGetInstanceProcAddr");
    }

    return pfnCreateVulkanDeviceKHR(instance, createInfo, vulkanDevice,
                                    vulkanResult);
  }

  struct VulkanResources {
    VkInstance Instance;
    VkPhysicalDevice PhysicalDevice;
    VkDevice Device;
    uint32_t QueueFamilyIndex;
  };
  VulkanResources
  createVulkan(const std::vector<const char *> &layers,
               const std::vector<const char *> &instanceExtensions,
               const std::vector<const char *> &deviceExtensions,
               const VkDebugUtilsMessengerCreateInfoEXT *debugInfo) {
    // Create the Vulkan device for the adapter associated with the system.
    // Extension function must be loaded by name
    XrGraphicsRequirementsVulkan2KHR graphicsRequirements{
        .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR,
    };
    XRO_CHECK(GetVulkanGraphicsRequirements2KHR(this->instance, this->systemId,
                                                &graphicsRequirements));

    for (auto name : layers) {
      Logger::Info("  vulkan layer: %s", name);
    }
    for (auto name : instanceExtensions) {
      Logger::Info("  vulkan instance extensions: %s", name);
    }
    for (auto name : deviceExtensions) {
      Logger::Info("  vulkan device extensions: %s", name);
    }

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "hello_xr",
        .applicationVersion = 1,
        .pEngineName = "hello_xr",
        .engineVersion = 1,
        .apiVersion = VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo instInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = debugInfo,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = (uint32_t)layers.size(),
        .ppEnabledLayerNames = layers.empty() ? nullptr : layers.data(),
        .enabledExtensionCount = (uint32_t)instanceExtensions.size(),
        .ppEnabledExtensionNames =
            instanceExtensions.empty() ? nullptr : instanceExtensions.data(),
    };

    XrVulkanInstanceCreateInfoKHR createInfo{
        .type = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR,
        .systemId = this->systemId,
        .pfnGetInstanceProcAddr = &vkGetInstanceProcAddr,
        .vulkanCreateInfo = &instInfo,
        .vulkanAllocator = nullptr,
    };

    VkInstance vkInstance;
    VkResult err;
    XRO_CHECK(CreateVulkanInstanceKHR(this->instance, &createInfo, &vkInstance,
                                      &err));

    XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo{
        .type = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR,
        .systemId = this->systemId,
        .vulkanInstance = vkInstance,
    };
    VkPhysicalDevice vkPhysicalDevice;
    XRO_CHECK(GetVulkanGraphicsDevice2KHR(this->instance, &deviceGetInfo,
                                          &vkPhysicalDevice));

    float queuePriorities = 0;
    VkDeviceQueueCreateInfo queueInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = 1,
        .pQueuePriorities = &queuePriorities,
    };
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice,
                                             &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        vkPhysicalDevice, &queueFamilyCount, &queueFamilyProps[0]);
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
      // Only need graphics (not presentation) for draw queue
      if ((queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
        queueInfo.queueFamilyIndex = i;
        break;
      }
    }

    VkPhysicalDeviceFeatures features{
    // features.samplerAnisotropy = VK_TRUE;
#ifndef ANDROID
        // quest3 not work
        .shaderStorageImageMultisample = VK_TRUE,
#endif
    };
    VkDeviceCreateInfo deviceInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = (uint32_t)deviceExtensions.size(),
        .ppEnabledExtensionNames =
            deviceExtensions.empty() ? nullptr : deviceExtensions.data(),
        .pEnabledFeatures = &features,
    };

    XrVulkanDeviceCreateInfoKHR deviceCreateInfo{
        .type = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR,
        .systemId = this->systemId,
        .pfnGetInstanceProcAddr = &vkGetInstanceProcAddr,
        .vulkanPhysicalDevice = vkPhysicalDevice,
        .vulkanCreateInfo = &deviceInfo,
        .vulkanAllocator = nullptr,
    };
    VkDevice vkDevice;
    XRO_CHECK(CreateVulkanDeviceKHR(this->instance, &deviceCreateInfo,
                                    &vkDevice, &err));
    vko::VKO_CHECK(err);

    return {
        .Instance = vkInstance,
        .PhysicalDevice = vkPhysicalDevice,
        .Device = vkDevice,
        .QueueFamilyIndex = queueInfo.queueFamilyIndex,
    };
  }
};

} // namespace xro

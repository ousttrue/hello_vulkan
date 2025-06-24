#pragma once

#include "../vk.h"

#ifdef XR_USE_PLATFORM_ANDROID
#include <android_native_app_glue.h>
#endif
#ifdef XR_USE_PLATFORM_WIN32
#include <Unknwn.h>
#endif

#include <openxr/openxr_platform.h>

namespace vuloxr {
namespace xr {

static XrResult GetVulkanGraphicsRequirements2KHR(
    XrInstance instance, XrSystemId systemId,
    XrGraphicsRequirementsVulkan2KHR *graphicsRequirements) {
  PFN_xrGetVulkanGraphicsRequirements2KHR pfnGetVulkanGraphicsRequirements2KHR =
      nullptr;
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

std::tuple<vk::Instance, vk::PhysicalDevice, vk::Device>
createVulkan(XrInstance xr_instance, XrSystemId xr_systemId,
             PFN_vkDebugUtilsMessengerCallbackEXT pfnUserCallback = nullptr) {
  vk::Instance instance;
  instance.appInfo.pApplicationName = "hello_xr";
  instance.appInfo.pEngineName = "hello_xr";
  instance.debugUtilsMessengerCreateInfo.sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  instance.debugUtilsMessengerCreateInfo.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
  instance.debugUtilsMessengerCreateInfo.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  if (pfnUserCallback) {
    instance.debugUtilsMessengerCreateInfo.pfnUserCallback = pfnUserCallback;
  }
  vk::Device device;

#ifdef NDEBUG
#else
  instance.debugUtilsMessengerCreateInfo.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
  instance.addLayer("VK_LAYER_KHRONOS_validation") ||
      instance.addLayer("VK_LAYER_LUNARG_standard_validation");
  instance.addExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) ||
      instance.addExtension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

  // Create the Vulkan device for the adapter associated with the system.
  // Extension function must be loaded by name
  XrGraphicsRequirementsVulkan2KHR graphicsRequirements{
      .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR,
  };
  CheckXrResult(GetVulkanGraphicsRequirements2KHR(xr_instance, xr_systemId,
                                                  &graphicsRequirements));

  for (auto name : instance.layers) {
    Logger::Info("  vulkan layer: %s", name);
  }
  for (auto name : instance.extensions) {
    Logger::Info("  vulkan instance extensions: %s", name);
  }
  for (auto name : device.extensions) {
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
      .pNext = &instance.debugUtilsMessengerCreateInfo,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = static_cast<uint32_t>(instance.layers.size()),
      .ppEnabledLayerNames = instance.layers.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(instance.extensions.size()),
      .ppEnabledExtensionNames = instance.extensions.data(),
  };

  XrVulkanInstanceCreateInfoKHR createInfo{
      .type = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR,
      .systemId = xr_systemId,
      .pfnGetInstanceProcAddr = &vkGetInstanceProcAddr,
      .vulkanCreateInfo = &instInfo,
      .vulkanAllocator = nullptr,
  };

  VkInstance vkInstance;
  VkResult err;
  CheckXrResult(
      CreateVulkanInstanceKHR(xr_instance, &createInfo, &vkInstance, &err));
  instance.reset(vkInstance);

  XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo{
      .type = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR,
      .systemId = xr_systemId,
      .vulkanInstance = vkInstance,
  };
  VkPhysicalDevice vkPhysicalDevice;
  CheckXrResult(GetVulkanGraphicsDevice2KHR(xr_instance, &deviceGetInfo,
                                            &vkPhysicalDevice));

  float queuePriorities = 0;
  VkDeviceQueueCreateInfo queueInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueCount = 1,
      .pQueuePriorities = &queuePriorities,
  };
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamilyCount,
                                           nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamilyCount,
                                           &queueFamilyProps[0]);
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
      .enabledLayerCount = static_cast<uint32_t>(instance.layers.size()),
      .ppEnabledLayerNames = instance.layers.data(),
      .enabledExtensionCount = static_cast<uint32_t>(device.extensions.size()),
      .ppEnabledExtensionNames = device.extensions.data(),
      .pEnabledFeatures = &features,
  };

  XrVulkanDeviceCreateInfoKHR deviceCreateInfo{
      .type = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR,
      .systemId = xr_systemId,
      .pfnGetInstanceProcAddr = &vkGetInstanceProcAddr,
      .vulkanPhysicalDevice = vkPhysicalDevice,
      .vulkanCreateInfo = &deviceInfo,
      .vulkanAllocator = nullptr,
  };
  VkDevice vkDevice;
  CheckXrResult(CreateVulkanDeviceKHR(xr_instance, &deviceCreateInfo,
                                      &vkDevice, &err));
  device.reset(vkDevice, queueInfo.queueFamilyIndex);
  vk::CheckVkResult(err);

  return {std::move(instance), vk::PhysicalDevice(vkPhysicalDevice),
          std::move(device)};
}

} // namespace xr
} // namespace vuloxr

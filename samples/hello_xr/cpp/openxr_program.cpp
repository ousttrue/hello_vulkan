// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include <vulkan/vulkan.h>
#ifdef XR_USE_PLATFORM_ANDROID
#include <android_native_app_glue.h>
#endif
#ifdef XR_USE_PLATFORM_WIN32
#include <Unknwn.h>
#endif
#include <openxr/openxr_platform.h>

#include "SwapchainImageContext.h"
#include "VulkanGraphicsPlugin.h"
#include "check.h"
#include "logger.h"
#include "openxr/openxr.h"
#include "openxr_program.h"
#include "options.h"
#include "to_string.h"
#include "vulkan_debug_object_namer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <common/xr_linear.h>
#include <set>

#if !defined(XR_USE_PLATFORM_WIN32)
#define strcpy_s(dest, source) strncpy((dest), (source), sizeof(dest))
#endif

static void LogLayersAndExtensions() {
  // Write out extension properties for a given layer.
  const auto logExtensions = [](const char *layerName, int indent = 0) {
    uint32_t instanceExtensionCount;
    if (xrEnumerateInstanceExtensionProperties(
            layerName, 0, &instanceExtensionCount, nullptr) != XR_SUCCESS) {
      throw std::runtime_error("xrEnumerateInstanceExtensionProperties");
    }
    std::vector<XrExtensionProperties> extensions(
        instanceExtensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
    if (xrEnumerateInstanceExtensionProperties(
            layerName, (uint32_t)extensions.size(), &instanceExtensionCount,
            extensions.data()) != XR_SUCCESS) {
      throw std::runtime_error("xrEnumerateInstanceExtensionProperties");
    }

    const std::string indentStr(indent, ' ');
    Log::Write(Log::Level::Verbose,
               Fmt("%sAvailable Extensions: (%d)", indentStr.c_str(),
                   instanceExtensionCount));
    for (const XrExtensionProperties &extension : extensions) {
      Log::Write(Log::Level::Verbose,
                 Fmt("%s  Name=%s SpecVersion=%d", indentStr.c_str(),
                     extension.extensionName, extension.extensionVersion));
    }
  };

  // Log non-layer extensions (layerName==nullptr).
  logExtensions(nullptr);

  // Log layers and any of their extensions.
  {
    uint32_t layerCount;
    CHECK_XRCMD(xrEnumerateApiLayerProperties(0, &layerCount, nullptr));
    std::vector<XrApiLayerProperties> layers(layerCount,
                                             {XR_TYPE_API_LAYER_PROPERTIES});
    CHECK_XRCMD(xrEnumerateApiLayerProperties((uint32_t)layers.size(),
                                              &layerCount, layers.data()));

    Log::Write(Log::Level::Info, Fmt("Available Layers: (%d)", layerCount));
    for (const XrApiLayerProperties &layer : layers) {
      Log::Write(Log::Level::Verbose,
                 Fmt("  Name=%s SpecVersion=%s LayerVersion=%d Description=%s",
                     layer.layerName,
                     GetXrVersionString(layer.specVersion).c_str(),
                     layer.layerVersion, layer.description));
      logExtensions(layer.layerName, 4);
    }
  }
}

namespace Math {
namespace Pose {
XrPosef Identity() {
  XrPosef t{};
  t.orientation.w = 1;
  return t;
}

XrPosef Translation(const XrVector3f &translation) {
  XrPosef t = Identity();
  t.position = translation;
  return t;
}

XrPosef RotateCCWAboutYAxis(float radians, XrVector3f translation) {
  XrPosef t = Identity();
  t.orientation.x = 0.f;
  t.orientation.y = std::sin(radians * 0.5f);
  t.orientation.z = 0.f;
  t.orientation.w = std::cos(radians * 0.5f);
  t.position = translation;
  return t;
}
} // namespace Pose
} // namespace Math

inline XrReferenceSpaceCreateInfo
GetXrReferenceSpaceCreateInfo(const std::string &referenceSpaceTypeStr) {
  XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{
      XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::Identity();
  if (EqualsIgnoreCase(referenceSpaceTypeStr, "View")) {
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "ViewFront")) {
    // Render head-locked 2m in front of device.
    referenceSpaceCreateInfo.poseInReferenceSpace =
        Math::Pose::Translation({0.f, 0.f, -2.f}),
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "Local")) {
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "Stage")) {
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageLeft")) {
    referenceSpaceCreateInfo.poseInReferenceSpace =
        Math::Pose::RotateCCWAboutYAxis(0.f, {-2.f, 0.f, -2.f});
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageRight")) {
    referenceSpaceCreateInfo.poseInReferenceSpace =
        Math::Pose::RotateCCWAboutYAxis(0.f, {2.f, 0.f, -2.f});
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageLeftRotated")) {
    referenceSpaceCreateInfo.poseInReferenceSpace =
        Math::Pose::RotateCCWAboutYAxis(3.14f / 3.f, {-2.f, 0.5f, -2.f});
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageRightRotated")) {
    referenceSpaceCreateInfo.poseInReferenceSpace =
        Math::Pose::RotateCCWAboutYAxis(-3.14f / 3.f, {2.f, 0.5f, -2.f});
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  } else {
    throw std::invalid_argument(Fmt("Unknown reference space type '%s'",
                                    referenceSpaceTypeStr.c_str()));
  }
  return referenceSpaceCreateInfo;
}

//
// OpenXrProgram
//
OpenXrProgram::OpenXrProgram(const Options &options, XrInstance instance,
                             XrSystemId systemId)
    : m_options(options), m_instance(instance), m_systemId(systemId),
      m_acceptableBlendModes{XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
                             XR_ENVIRONMENT_BLEND_MODE_ADDITIVE,
                             XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND} {

  Log::Write(Log::Level::Verbose,
             Fmt("Using system %d for form factor %s", m_systemId,
                 to_string(m_options.Parsed.FormFactor)));
  CHECK(m_instance != XR_NULL_HANDLE);
  CHECK(m_systemId != XR_NULL_SYSTEM_ID);
}

OpenXrProgram::~OpenXrProgram() {
  for (Swapchain swapchain : m_swapchains) {
    xrDestroySwapchain(swapchain.handle);
  }

  if (m_input.actionSet != XR_NULL_HANDLE) {
    for (auto hand : {Side::LEFT, Side::RIGHT}) {
      xrDestroySpace(m_input.handSpace[hand]);
    }
    xrDestroyActionSet(m_input.actionSet);
  }

  for (XrSpace visualizedSpace : m_visualizedSpaces) {
    xrDestroySpace(visualizedSpace);
  }

  if (m_appSpace != XR_NULL_HANDLE) {
    xrDestroySpace(m_appSpace);
  }

  if (m_session != XR_NULL_HANDLE) {
    xrDestroySession(m_session);
  }

  if (m_instance != XR_NULL_HANDLE) {
    xrDestroyInstance(m_instance);
  }
}

// OpenXR extensions required by this graphics API.
static std::vector<const char *> GetInstanceExtensions() {
  return {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
}

std::shared_ptr<OpenXrProgram>
OpenXrProgram::Create(const Options &options,
                      const std::vector<std::string> &platformExtensions,
                      void *next) {
  LogLayersAndExtensions();

  // Create union of extensions required by platform and graphics plugins.
  std::vector<const char *> extensions;

  for (auto &ext : platformExtensions) {
    extensions.push_back(ext.c_str());
  }

  auto instanceExtensions = GetInstanceExtensions();
  for (auto &ext : instanceExtensions) {
    extensions.push_back(ext);
  }

  XrInstanceCreateInfo createInfo{
      .type = XR_TYPE_INSTANCE_CREATE_INFO,
      .next = next,
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
      .enabledExtensionCount = (uint32_t)extensions.size(),
      .enabledExtensionNames = extensions.data(),
  };
  XrInstance instance;
  CHECK_XRCMD(xrCreateInstance(&createInfo, &instance));
  CHECK(instance != XR_NULL_HANDLE);

  XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
  CHECK_XRCMD(xrGetInstanceProperties(instance, &instanceProperties));

  Log::Write(
      Log::Level::Info,
      Fmt("Instance RuntimeName=%s RuntimeVersion=%s",
          instanceProperties.runtimeName,
          GetXrVersionString(instanceProperties.runtimeVersion).c_str()));

  // Select a System for the view configuration specified in the Options
  CHECK(instance != XR_NULL_HANDLE);

  XrSystemGetInfo systemInfo{
      .type = XR_TYPE_SYSTEM_GET_INFO,
      .formFactor = options.Parsed.FormFactor,
  };
  XrSystemId systemId;
  if (xrGetSystem(instance, &systemInfo, &systemId) != XR_SUCCESS) {
    return {};
  }

  auto ptr = std::shared_ptr<OpenXrProgram>(
      new OpenXrProgram(options, instance, systemId));

  return ptr;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugMessageThunk(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                  const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                  void *pUserData) {
  return static_cast<VulkanGraphicsPlugin *>(pUserData)->debugMessage(
      messageSeverity, messageTypes, pCallbackData);
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

static void LogEnvironmentBlendMode(XrInstance m_instance,
                                    XrSystemId m_systemId,
                                    const Options &m_options,
                                    XrViewConfigurationType type) {
  CHECK(m_instance != XR_NULL_HANDLE);
  CHECK(m_systemId != 0);

  uint32_t count;
  CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, type, 0,
                                               &count, nullptr));
  CHECK(count > 0);

  Log::Write(Log::Level::Info,
             Fmt("Available Environment Blend Mode count : (%d)", count));

  std::vector<XrEnvironmentBlendMode> blendModes(count);
  CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(
      m_instance, m_systemId, type, count, &count, blendModes.data()));

  bool blendModeFound = false;
  for (XrEnvironmentBlendMode mode : blendModes) {
    const bool blendModeMatch = (mode == m_options.Parsed.EnvironmentBlendMode);
    Log::Write(Log::Level::Info,
               Fmt("Environment Blend Mode (%s) : %s", to_string(mode),
                   blendModeMatch ? "(Selected)" : ""));
    blendModeFound |= blendModeMatch;
  }
  CHECK(blendModeFound);
}

static void LogViewConfigurations(XrInstance m_instance, XrSystemId m_systemId,
                                  const Options &m_options) {
  CHECK(m_instance != XR_NULL_HANDLE);
  CHECK(m_systemId != XR_NULL_SYSTEM_ID);

  uint32_t viewConfigTypeCount;
  CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, 0,
                                            &viewConfigTypeCount, nullptr));
  std::vector<XrViewConfigurationType> viewConfigTypes(viewConfigTypeCount);
  CHECK_XRCMD(xrEnumerateViewConfigurations(
      m_instance, m_systemId, viewConfigTypeCount, &viewConfigTypeCount,
      viewConfigTypes.data()));
  CHECK((uint32_t)viewConfigTypes.size() == viewConfigTypeCount);

  Log::Write(Log::Level::Info, Fmt("Available View Configuration Types: (%d)",
                                   viewConfigTypeCount));
  for (XrViewConfigurationType viewConfigType : viewConfigTypes) {
    Log::Write(
        Log::Level::Verbose,
        Fmt("  View Configuration Type: %s %s", to_string(viewConfigType),
            viewConfigType == m_options.Parsed.ViewConfigType ? "(Selected)"
                                                              : ""));

    XrViewConfigurationProperties viewConfigProperties{
        .type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES,
    };
    CHECK_XRCMD(xrGetViewConfigurationProperties(
        m_instance, m_systemId, viewConfigType, &viewConfigProperties));

    Log::Write(
        Log::Level::Verbose,
        Fmt("  View configuration FovMutable=%s",
            viewConfigProperties.fovMutable == XR_TRUE ? "True" : "False"));

    uint32_t viewCount;
    CHECK_XRCMD(xrEnumerateViewConfigurationViews(
        m_instance, m_systemId, viewConfigType, 0, &viewCount, nullptr));
    if (viewCount > 0) {
      std::vector<XrViewConfigurationView> views(
          viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
      CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId,
                                                    viewConfigType, viewCount,
                                                    &viewCount, views.data()));

      for (uint32_t i = 0; i < views.size(); i++) {
        const XrViewConfigurationView &view = views[i];

        Log::Write(Log::Level::Verbose,
                   Fmt("    View [%d]: Recommended Width=%d Height=%d "
                       "SampleCount=%d",
                       i, view.recommendedImageRectWidth,
                       view.recommendedImageRectHeight,
                       view.recommendedSwapchainSampleCount));
        Log::Write(Log::Level::Verbose,
                   Fmt("    View [%d]:     Maximum Width=%d Height=%d "
                       "SampleCount=%d",
                       i, view.maxImageRectWidth, view.maxImageRectHeight,
                       view.maxSwapchainSampleCount));
      }
    } else {
      Log::Write(Log::Level::Error, Fmt("Empty view configuration type"));
    }

    LogEnvironmentBlendMode(m_instance, m_systemId, m_options, viewConfigType);
  }
}

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

std::shared_ptr<VulkanGraphicsPlugin> OpenXrProgram::InitializeDevice(
    const std::vector<const char *> &layers,
    const std::vector<const char *> &instanceExtensions,
    const std::vector<const char *> &deviceExtensions) {
  LogViewConfigurations(m_instance, m_systemId, m_options);

  // Create the Vulkan device for the adapter associated with the system.
  // Extension function must be loaded by name
  XrGraphicsRequirementsVulkan2KHR graphicsRequirements{
      .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR,
  };
  if (GetVulkanGraphicsRequirements2KHR(m_instance, m_systemId,
                                        &graphicsRequirements) != XR_SUCCESS) {
    throw std::runtime_error("GetVulkanGraphicsRequirements2KHR");
  }

  VkDebugUtilsMessengerCreateInfoEXT debugInfo{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = static_cast<VkDebugUtilsMessageSeverityFlagsEXT>(
          layers.empty() ? VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                         : VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT),
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debugMessageThunk,
      .pUserData = this,
  };

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
      .pNext = &debugInfo,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = (uint32_t)layers.size(),
      .ppEnabledLayerNames = layers.empty() ? nullptr : layers.data(),
      .enabledExtensionCount = (uint32_t)instanceExtensions.size(),
      .ppEnabledExtensionNames =
          instanceExtensions.empty() ? nullptr : instanceExtensions.data(),
  };

  XrVulkanInstanceCreateInfoKHR createInfo{
      .type = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR,
      .systemId = m_systemId,
      .pfnGetInstanceProcAddr = &vkGetInstanceProcAddr,
      .vulkanCreateInfo = &instInfo,
      .vulkanAllocator = nullptr,
  };

  VkInstance vkInstance;
  VkResult err;
  if (CreateVulkanInstanceKHR(m_instance, &createInfo, &vkInstance, &err) !=
      XR_SUCCESS) {
    throw std::runtime_error("CreateVulkanInstanceKHR");
  }
  if (err != VK_SUCCESS) {
    throw std::runtime_error("CreateVulkanInstanceKHR");
  }
  SetDebugUtilsObjectNameEXT_GetProc(vkInstance);

  XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo{
      .type = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR,
      .systemId = m_systemId,
      .vulkanInstance = vkInstance,
  };
  VkPhysicalDevice vkPhysicalDevice;
  if (GetVulkanGraphicsDevice2KHR(m_instance, &deviceGetInfo,
                                  &vkPhysicalDevice) != XR_SUCCESS) {
    throw std::runtime_error("GetVulkanGraphicsDevice2KHR");
  }

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

  uint32_t queueFamilyIndex = UINT_MAX;
  for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    // Only need graphics (not presentation) for draw queue
    if ((queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
      queueFamilyIndex = queueInfo.queueFamilyIndex = i;
      break;
    }
  }

  VkPhysicalDeviceFeatures features{};
  // features.samplerAnisotropy = VK_TRUE;
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
      .systemId = m_systemId,
      .pfnGetInstanceProcAddr = &vkGetInstanceProcAddr,
      .vulkanPhysicalDevice = vkPhysicalDevice,
      .vulkanCreateInfo = &deviceInfo,
      .vulkanAllocator = nullptr,
  };
  VkDevice vkDevice;
  if (CreateVulkanDeviceKHR(m_instance, &deviceCreateInfo, &vkDevice, &err) !=
      XR_SUCCESS) {
    throw std::runtime_error("CreateVulkanDeviceKHR");
  }
  if (err != VK_SUCCESS) {
    throw std::runtime_error("CreateVulkanDeviceKHR");
  }

  // The graphics API can initialize the graphics device now that the systemId
  // and instance handle are available.
  return std::make_shared<VulkanGraphicsPlugin>(
      vkInstance, vkPhysicalDevice, vkDevice, queueInfo.queueFamilyIndex,
      debugInfo);
}

static void LogReferenceSpaces(XrSession m_session) {

  CHECK(m_session != XR_NULL_HANDLE);

  uint32_t spaceCount;
  CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, 0, &spaceCount, nullptr));
  std::vector<XrReferenceSpaceType> spaces(spaceCount);
  CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, spaceCount, &spaceCount,
                                         spaces.data()));

  Log::Write(Log::Level::Info,
             Fmt("Available reference spaces: %d", spaceCount));
  for (XrReferenceSpaceType space : spaces) {
    Log::Write(Log::Level::Verbose, Fmt("  Name: %s", to_string(space)));
  }
}

void OpenXrProgram::InitializeSession(
    const std::shared_ptr<VulkanGraphicsPlugin> &vulkan) {
  CHECK(m_instance != XR_NULL_HANDLE);
  CHECK(m_session == XR_NULL_HANDLE);

  {
    Log::Write(Log::Level::Verbose, Fmt("Creating session..."));

    XrGraphicsBindingVulkan2KHR graphicsBinding{
        .type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR,
        .next = nullptr,
        .instance = vulkan->m_vkInstance,
        .physicalDevice = vulkan->m_vkPhysicalDevice,
        .device = vulkan->m_vkDevice,
        .queueFamilyIndex = vulkan->m_queueFamilyIndex,
        .queueIndex = 0,
    };
    XrSessionCreateInfo createInfo{
        .type = XR_TYPE_SESSION_CREATE_INFO,
        .next = &graphicsBinding,
        .systemId = m_systemId,
    };
    CHECK_XRCMD(xrCreateSession(m_instance, &createInfo, &m_session));
  }

  LogReferenceSpaces(m_session);
  InitializeActions();
  CreateVisualizedSpaces();

  {
    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo =
        GetXrReferenceSpaceCreateInfo(m_options.AppSpace);
    CHECK_XRCMD(xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo,
                                       &m_appSpace));
  }
}

void OpenXrProgram::CreateSwapchains(
    const std::shared_ptr<VulkanGraphicsPlugin> &vulkan) {
  CHECK(m_session != XR_NULL_HANDLE);

  // Read graphics properties for preferred swapchain length and logging.
  XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
  CHECK_XRCMD(xrGetSystemProperties(m_instance, m_systemId, &systemProperties));

  // Log system properties.
  Log::Write(Log::Level::Info,
             Fmt("System Properties: Name=%s VendorId=%d",
                 systemProperties.systemName, systemProperties.vendorId));
  Log::Write(
      Log::Level::Info,
      Fmt("System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
          systemProperties.graphicsProperties.maxSwapchainImageWidth,
          systemProperties.graphicsProperties.maxSwapchainImageHeight,
          systemProperties.graphicsProperties.maxLayerCount));
  Log::Write(
      Log::Level::Info,
      Fmt("System Tracking Properties: OrientationTracking=%s "
          "PositionTracking=%s",
          systemProperties.trackingProperties.orientationTracking == XR_TRUE
              ? "True"
              : "False",
          systemProperties.trackingProperties.positionTracking == XR_TRUE
              ? "True"
              : "False"));

  // Note: No other view configurations exist at the time this code was
  // written. If this condition is not met, the project will need to be
  // audited to see how support should be added.
  CHECK_MSG(m_options.Parsed.ViewConfigType ==
                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            "Unsupported view configuration type");

  // return ProjectionLayer::Create(m_instance, m_systemId, m_session,
  //                                m_options.Parsed.ViewConfigType, vulkan);
  // }
  // std::shared_ptr<ProjectionLayer> ProjectionLayer::Create(
  //     XrInstance instance, XrSystemId systemId, XrSession session,
  //     XrViewConfigurationType viewConfigurationType,
  //     const std::shared_ptr<class VulkanGraphicsPlugin> &vulkan) {
  // auto ptr = std::shared_ptr<ProjectionLayer>(new ProjectionLayer);

  // Query and cache view configuration views.
  uint32_t viewCount;
  CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId,
                                                m_options.Parsed.ViewConfigType,
                                                0, &viewCount, nullptr));
  m_configViews.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  CHECK_XRCMD(xrEnumerateViewConfigurationViews(
      m_instance, m_systemId, m_options.Parsed.ViewConfigType, viewCount,
      &viewCount, m_configViews.data()));

  // Create and cache view buffer for xrLocateViews later.
  m_views.resize(viewCount, {XR_TYPE_VIEW});

  // Create the swapchain and get the images.
  if (viewCount > 0) {
    // Select a swapchain format.
    uint32_t swapchainFormatCount;
    CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, 0, &swapchainFormatCount,
                                            nullptr));
    std::vector<int64_t> swapchainFormats(swapchainFormatCount);
    CHECK_XRCMD(xrEnumerateSwapchainFormats(
        m_session, (uint32_t)swapchainFormats.size(), &swapchainFormatCount,
        swapchainFormats.data()));
    CHECK(swapchainFormatCount == swapchainFormats.size());
    m_colorSwapchainFormat =
        vulkan->SelectColorSwapchainFormat(swapchainFormats);

    // Print swapchain formats and the selected one.
    {
      std::string swapchainFormatsString;
      for (int64_t format : swapchainFormats) {
        const bool selected = format == m_colorSwapchainFormat;
        swapchainFormatsString += " ";
        if (selected) {
          swapchainFormatsString += "[";
        }
        swapchainFormatsString += std::to_string(format);
        if (selected) {
          swapchainFormatsString += "]";
        }
      }
      Log::Write(Log::Level::Verbose,
                 Fmt("Swapchain Formats: %s", swapchainFormatsString.c_str()));
    }

    // Create a swapchain for each view.
    for (uint32_t i = 0; i < viewCount; i++) {
      const XrViewConfigurationView &vp = m_configViews[i];
      Log::Write(Log::Level::Info,
                 Fmt("Creating swapchain for view %d with dimensions "
                     "Width=%d Height=%d SampleCount=%d",
                     i, vp.recommendedImageRectWidth,
                     vp.recommendedImageRectHeight,
                     vp.recommendedSwapchainSampleCount));

      // Create the swapchain.
      XrSwapchainCreateInfo swapchainCreateInfo{
          .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
          .createFlags = 0,
          .usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
          .format = m_colorSwapchainFormat,
          .sampleCount = VK_SAMPLE_COUNT_1_BIT,
          .width = vp.recommendedImageRectWidth,
          .height = vp.recommendedImageRectHeight,
          .faceCount = 1,
          .arraySize = 1,
          .mipCount = 1,
      };

      OpenXrProgram::Swapchain swapchain{
          .extent =
              {
                  .width = static_cast<int32_t>(swapchainCreateInfo.width),
                  .height = static_cast<int32_t>(swapchainCreateInfo.height),
              },
      };
      CHECK_XRCMD(xrCreateSwapchain(m_session, &swapchainCreateInfo,
                                    &swapchain.handle));
      m_swapchains.push_back(swapchain);

      uint32_t imageCount;
      CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount,
                                             nullptr));
      // XXX This should really just return XrSwapchainImageBaseHeader*
      std::vector<XrSwapchainImageBaseHeader *> swapchainImages =
          AllocateSwapchainImageStructs(
              imageCount,
              {swapchainCreateInfo.width, swapchainCreateInfo.height},
              static_cast<VkFormat>(swapchainCreateInfo.format),
              static_cast<VkSampleCountFlagBits>(
                  swapchainCreateInfo.sampleCount),
              vulkan);
      CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, imageCount,
                                             &imageCount, swapchainImages[0]));

      m_swapchainImages.insert(
          std::make_pair(swapchain.handle, std::move(swapchainImages)));
    }
  }

  // return ptr;
}

void OpenXrProgram::PollEvents(bool *exitRenderLoop, bool *requestRestart) {
  *exitRenderLoop = *requestRestart = false;

  // Process all pending messages.
  while (const XrEventDataBaseHeader *event = TryReadNextEvent()) {
    switch (event->type) {
    case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
      const auto &instanceLossPending =
          *reinterpret_cast<const XrEventDataInstanceLossPending *>(event);
      Log::Write(Log::Level::Warning,
                 Fmt("XrEventDataInstanceLossPending by %lld",
                     instanceLossPending.lossTime));
      *exitRenderLoop = true;
      *requestRestart = true;
      return;
    }
    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
      auto sessionStateChangedEvent =
          *reinterpret_cast<const XrEventDataSessionStateChanged *>(event);
      HandleSessionStateChangedEvent(sessionStateChangedEvent, exitRenderLoop,
                                     requestRestart);
      break;
    }
    case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
      LogActionSourceName(m_input.grabAction, "Grab");
      LogActionSourceName(m_input.quitAction, "Quit");
      LogActionSourceName(m_input.poseAction, "Pose");
      LogActionSourceName(m_input.vibrateAction, "Vibrate");
      break;
    case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
    default: {
      Log::Write(Log::Level::Verbose,
                 Fmt("Ignoring event type %d", event->type));
      break;
    }
    }
  }
}

void OpenXrProgram::PollActions() {
  m_input.handActive = {XR_FALSE, XR_FALSE};

  // Sync actions
  const XrActiveActionSet activeActionSet{m_input.actionSet, XR_NULL_PATH};
  XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
  syncInfo.countActiveActionSets = 1;
  syncInfo.activeActionSets = &activeActionSet;
  CHECK_XRCMD(xrSyncActions(m_session, &syncInfo));

  // Get pose and grab action state and start haptic vibrate when hand is 90%
  // squeezed.
  for (auto hand : {Side::LEFT, Side::RIGHT}) {
    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = m_input.grabAction;
    getInfo.subactionPath = m_input.handSubactionPath[hand];

    XrActionStateFloat grabValue{XR_TYPE_ACTION_STATE_FLOAT};
    CHECK_XRCMD(xrGetActionStateFloat(m_session, &getInfo, &grabValue));
    if (grabValue.isActive == XR_TRUE) {
      // Scale the rendered hand by 1.0f (open) to 0.5f (fully squeezed).
      m_input.handScale[hand] = 1.0f - 0.5f * grabValue.currentState;
      if (grabValue.currentState > 0.9f) {
        XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
        vibration.amplitude = 0.5;
        vibration.duration = XR_MIN_HAPTIC_DURATION;
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

        XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
        hapticActionInfo.action = m_input.vibrateAction;
        hapticActionInfo.subactionPath = m_input.handSubactionPath[hand];
        CHECK_XRCMD(xrApplyHapticFeedback(m_session, &hapticActionInfo,
                                          (XrHapticBaseHeader *)&vibration));
      }
    }

    getInfo.action = m_input.poseAction;
    XrActionStatePose poseState{XR_TYPE_ACTION_STATE_POSE};
    CHECK_XRCMD(xrGetActionStatePose(m_session, &getInfo, &poseState));
    m_input.handActive[hand] = poseState.isActive;
  }

  // There were no subaction paths specified for the quit action, because we
  // don't care which hand did it.
  XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr,
                               m_input.quitAction, XR_NULL_PATH};
  XrActionStateBoolean quitValue{XR_TYPE_ACTION_STATE_BOOLEAN};
  CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &quitValue));
  if ((quitValue.isActive == XR_TRUE) &&
      (quitValue.changedSinceLastSync == XR_TRUE) &&
      (quitValue.currentState == XR_TRUE)) {
    CHECK_XRCMD(xrRequestExitSession(m_session));
  }
}

XrFrameState OpenXrProgram::BeginFrame() {
  CHECK(m_session != XR_NULL_HANDLE);

  XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
  XrFrameState frameState{XR_TYPE_FRAME_STATE};
  CHECK_XRCMD(xrWaitFrame(m_session, &frameWaitInfo, &frameState));

  XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
  CHECK_XRCMD(xrBeginFrame(m_session, &frameBeginInfo));

  return frameState;
}

void OpenXrProgram::EndFrame(
    XrTime predictedDisplayTime,
    const std::vector<XrCompositionLayerBaseHeader *> &layers) {
  XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
  frameEndInfo.displayTime = predictedDisplayTime;
  frameEndInfo.environmentBlendMode = m_options.Parsed.EnvironmentBlendMode;
  frameEndInfo.layerCount = (uint32_t)layers.size();
  frameEndInfo.layers = layers.data();
  CHECK_XRCMD(xrEndFrame(m_session, &frameEndInfo));
}

XrEnvironmentBlendMode OpenXrProgram::GetPreferredBlendMode() const {
  uint32_t count;
  CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId,
                                               m_options.Parsed.ViewConfigType,
                                               0, &count, nullptr));
  CHECK(count > 0);

  std::vector<XrEnvironmentBlendMode> blendModes(count);
  CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(
      m_instance, m_systemId, m_options.Parsed.ViewConfigType, count, &count,
      blendModes.data()));
  for (const auto &blendMode : blendModes) {
    if (m_acceptableBlendModes.count(blendMode))
      return blendMode;
  }
  THROW("No acceptable blend mode returned from the "
        "xrEnumerateEnvironmentBlendModes");
}

void OpenXrProgram::InitializeActions() {
  // Create an action set.
  {
    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy_s(actionSetInfo.actionSetName, "gameplay");
    strcpy_s(actionSetInfo.localizedActionSetName, "Gameplay");
    actionSetInfo.priority = 0;
    CHECK_XRCMD(
        xrCreateActionSet(m_instance, &actionSetInfo, &m_input.actionSet));
  }

  // Get the XrPath for the left and right hands - we will use them as
  // subaction paths.
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left",
                             &m_input.handSubactionPath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right",
                             &m_input.handSubactionPath[Side::RIGHT]));

  // Create actions.
  {
    // Create an input action for grabbing objects with the left and right
    // hands.
    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
    actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
    strcpy_s(actionInfo.actionName, "grab_object");
    strcpy_s(actionInfo.localizedActionName, "Grab Object");
    actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
    actionInfo.subactionPaths = m_input.handSubactionPath.data();
    CHECK_XRCMD(
        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.grabAction));

    // Create an input action getting the left and right hand poses.
    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
    strcpy_s(actionInfo.actionName, "hand_pose");
    strcpy_s(actionInfo.localizedActionName, "Hand Pose");
    actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
    actionInfo.subactionPaths = m_input.handSubactionPath.data();
    CHECK_XRCMD(
        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.poseAction));

    // Create output actions for vibrating the left and right controller.
    actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
    strcpy_s(actionInfo.actionName, "vibrate_hand");
    strcpy_s(actionInfo.localizedActionName, "Vibrate Hand");
    actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
    actionInfo.subactionPaths = m_input.handSubactionPath.data();
    CHECK_XRCMD(
        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.vibrateAction));

    // Create input actions for quitting the session using the left and right
    // controller. Since it doesn't matter which hand did this, we do not
    // specify subaction paths for it. We will just suggest bindings for both
    // hands, where possible.
    actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    strcpy_s(actionInfo.actionName, "quit_session");
    strcpy_s(actionInfo.localizedActionName, "Quit Session");
    actionInfo.countSubactionPaths = 0;
    actionInfo.subactionPaths = nullptr;
    CHECK_XRCMD(
        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.quitAction));
  }

  std::array<XrPath, Side::COUNT> selectPath;
  std::array<XrPath, Side::COUNT> squeezeValuePath;
  std::array<XrPath, Side::COUNT> squeezeForcePath;
  std::array<XrPath, Side::COUNT> squeezeClickPath;
  std::array<XrPath, Side::COUNT> posePath;
  std::array<XrPath, Side::COUNT> hapticPath;
  std::array<XrPath, Side::COUNT> menuClickPath;
  std::array<XrPath, Side::COUNT> bClickPath;
  std::array<XrPath, Side::COUNT> triggerValuePath;
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/select/click",
                             &selectPath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/select/click",
                             &selectPath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/value",
                             &squeezeValuePath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/value",
                             &squeezeValuePath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/force",
                             &squeezeForcePath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/force",
                             &squeezeForcePath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/click",
                             &squeezeClickPath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/click",
                             &squeezeClickPath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/grip/pose",
                             &posePath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/grip/pose",
                             &posePath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/output/haptic",
                             &hapticPath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/output/haptic",
                             &hapticPath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/menu/click",
                             &menuClickPath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/menu/click",
                             &menuClickPath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/b/click",
                             &bClickPath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/b/click",
                             &bClickPath[Side::RIGHT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/trigger/value",
                             &triggerValuePath[Side::LEFT]));
  CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/trigger/value",
                             &triggerValuePath[Side::RIGHT]));
  // Suggest bindings for KHR Simple.
  {
    XrPath khrSimpleInteractionProfilePath;
    CHECK_XRCMD(xrStringToPath(m_instance,
                               "/interaction_profiles/khr/simple_controller",
                               &khrSimpleInteractionProfilePath));
    std::vector<XrActionSuggestedBinding> bindings{
        {// Fall back to a click input for the grab action.
         {m_input.grabAction, selectPath[Side::LEFT]},
         {m_input.grabAction, selectPath[Side::RIGHT]},
         {m_input.poseAction, posePath[Side::LEFT]},
         {m_input.poseAction, posePath[Side::RIGHT]},
         {m_input.quitAction, menuClickPath[Side::LEFT]},
         {m_input.quitAction, menuClickPath[Side::RIGHT]},
         {m_input.vibrateAction, hapticPath[Side::LEFT]},
         {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};
    XrInteractionProfileSuggestedBinding suggestedBindings{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = khrSimpleInteractionProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    CHECK_XRCMD(
        xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
  }
  // Suggest bindings for the Oculus Touch.
  {
    XrPath oculusTouchInteractionProfilePath;
    CHECK_XRCMD(xrStringToPath(m_instance,
                               "/interaction_profiles/oculus/touch_controller",
                               &oculusTouchInteractionProfilePath));
    std::vector<XrActionSuggestedBinding> bindings{
        {{m_input.grabAction, squeezeValuePath[Side::LEFT]},
         {m_input.grabAction, squeezeValuePath[Side::RIGHT]},
         {m_input.poseAction, posePath[Side::LEFT]},
         {m_input.poseAction, posePath[Side::RIGHT]},
         {m_input.quitAction, menuClickPath[Side::LEFT]},
         {m_input.vibrateAction, hapticPath[Side::LEFT]},
         {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};
    XrInteractionProfileSuggestedBinding suggestedBindings{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = oculusTouchInteractionProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    CHECK_XRCMD(
        xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
  }
  // Suggest bindings for the Vive Controller.
  {
    XrPath viveControllerInteractionProfilePath;
    CHECK_XRCMD(xrStringToPath(m_instance,
                               "/interaction_profiles/htc/vive_controller",
                               &viveControllerInteractionProfilePath));
    std::vector<XrActionSuggestedBinding> bindings{
        {{m_input.grabAction, triggerValuePath[Side::LEFT]},
         {m_input.grabAction, triggerValuePath[Side::RIGHT]},
         {m_input.poseAction, posePath[Side::LEFT]},
         {m_input.poseAction, posePath[Side::RIGHT]},
         {m_input.quitAction, menuClickPath[Side::LEFT]},
         {m_input.quitAction, menuClickPath[Side::RIGHT]},
         {m_input.vibrateAction, hapticPath[Side::LEFT]},
         {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};
    XrInteractionProfileSuggestedBinding suggestedBindings{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = viveControllerInteractionProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    CHECK_XRCMD(
        xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
  }

  // Suggest bindings for the Valve Index Controller.
  {
    XrPath indexControllerInteractionProfilePath;
    CHECK_XRCMD(xrStringToPath(m_instance,
                               "/interaction_profiles/valve/index_controller",
                               &indexControllerInteractionProfilePath));
    std::vector<XrActionSuggestedBinding> bindings{
        {{m_input.grabAction, squeezeForcePath[Side::LEFT]},
         {m_input.grabAction, squeezeForcePath[Side::RIGHT]},
         {m_input.poseAction, posePath[Side::LEFT]},
         {m_input.poseAction, posePath[Side::RIGHT]},
         {m_input.quitAction, bClickPath[Side::LEFT]},
         {m_input.quitAction, bClickPath[Side::RIGHT]},
         {m_input.vibrateAction, hapticPath[Side::LEFT]},
         {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};
    XrInteractionProfileSuggestedBinding suggestedBindings{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile =
        indexControllerInteractionProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    CHECK_XRCMD(
        xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
  }

  // Suggest bindings for the Microsoft Mixed Reality Motion Controller.
  {
    XrPath microsoftMixedRealityInteractionProfilePath;
    CHECK_XRCMD(xrStringToPath(
        m_instance, "/interaction_profiles/microsoft/motion_controller",
        &microsoftMixedRealityInteractionProfilePath));
    std::vector<XrActionSuggestedBinding> bindings{
        {{m_input.grabAction, squeezeClickPath[Side::LEFT]},
         {m_input.grabAction, squeezeClickPath[Side::RIGHT]},
         {m_input.poseAction, posePath[Side::LEFT]},
         {m_input.poseAction, posePath[Side::RIGHT]},
         {m_input.quitAction, menuClickPath[Side::LEFT]},
         {m_input.quitAction, menuClickPath[Side::RIGHT]},
         {m_input.vibrateAction, hapticPath[Side::LEFT]},
         {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};
    XrInteractionProfileSuggestedBinding suggestedBindings{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile =
        microsoftMixedRealityInteractionProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    CHECK_XRCMD(
        xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
  }
  XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
  actionSpaceInfo.action = m_input.poseAction;
  actionSpaceInfo.poseInActionSpace.orientation.w = 1.f;
  actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::LEFT];
  CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo,
                                  &m_input.handSpace[Side::LEFT]));
  actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::RIGHT];
  CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo,
                                  &m_input.handSpace[Side::RIGHT]));

  XrSessionActionSetsAttachInfo attachInfo{
      XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attachInfo.countActionSets = 1;
  attachInfo.actionSets = &m_input.actionSet;
  CHECK_XRCMD(xrAttachSessionActionSets(m_session, &attachInfo));
}

void OpenXrProgram::CreateVisualizedSpaces() {
  CHECK(m_session != XR_NULL_HANDLE);

  std::string visualizedSpaces[] = {
      "ViewFront",        "Local",      "Stage",
      "StageLeft",        "StageRight", "StageLeftRotated",
      "StageRightRotated"};

  for (const auto &visualizedSpace : visualizedSpaces) {
    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo =
        GetXrReferenceSpaceCreateInfo(visualizedSpace);
    XrSpace space;
    XrResult res =
        xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &space);
    if (XR_SUCCEEDED(res)) {
      m_visualizedSpaces.push_back(space);
    } else {
      Log::Write(Log::Level::Warning,
                 Fmt("Failed to create reference space %s with error %d",
                     visualizedSpace.c_str(), res));
    }
  }
}

// Return event if one is available, otherwise return null.
const XrEventDataBaseHeader *OpenXrProgram::TryReadNextEvent() {
  // It is sufficient to clear the just the XrEventDataBuffer header to
  // XR_TYPE_EVENT_DATA_BUFFER
  XrEventDataBaseHeader *baseHeader =
      reinterpret_cast<XrEventDataBaseHeader *>(&m_eventDataBuffer);
  *baseHeader = {XR_TYPE_EVENT_DATA_BUFFER};
  const XrResult xr = xrPollEvent(m_instance, &m_eventDataBuffer);
  if (xr == XR_SUCCESS) {
    if (baseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
      const XrEventDataEventsLost *const eventsLost =
          reinterpret_cast<const XrEventDataEventsLost *>(baseHeader);
      Log::Write(Log::Level::Warning,
                 Fmt("%d events lost", eventsLost->lostEventCount));
    }

    return baseHeader;
  }
  if (xr == XR_EVENT_UNAVAILABLE) {
    return nullptr;
  }
  THROW_XR(xr, "xrPollEvent");
}

void OpenXrProgram::HandleSessionStateChangedEvent(
    const XrEventDataSessionStateChanged &stateChangedEvent,
    bool *exitRenderLoop, bool *requestRestart) {
  const XrSessionState oldState = m_sessionState;
  m_sessionState = stateChangedEvent.state;

  Log::Write(Log::Level::Info,
             Fmt("XrEventDataSessionStateChanged: state %s->%s session=%lld "
                 "time=%lld",
                 to_string(oldState), to_string(m_sessionState),
                 stateChangedEvent.session, stateChangedEvent.time));

  if ((stateChangedEvent.session != XR_NULL_HANDLE) &&
      (stateChangedEvent.session != m_session)) {
    Log::Write(Log::Level::Error,
               "XrEventDataSessionStateChanged for unknown session");
    return;
  }

  switch (m_sessionState) {
  case XR_SESSION_STATE_READY: {
    CHECK(m_session != XR_NULL_HANDLE);
    XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
    sessionBeginInfo.primaryViewConfigurationType =
        m_options.Parsed.ViewConfigType;
    CHECK_XRCMD(xrBeginSession(m_session, &sessionBeginInfo));
    m_sessionRunning = true;
    break;
  }
  case XR_SESSION_STATE_STOPPING: {
    CHECK(m_session != XR_NULL_HANDLE);
    m_sessionRunning = false;
    CHECK_XRCMD(xrEndSession(m_session))
    break;
  }
  case XR_SESSION_STATE_EXITING: {
    *exitRenderLoop = true;
    // Do not attempt to restart because user closed this session.
    *requestRestart = false;
    break;
  }
  case XR_SESSION_STATE_LOSS_PENDING: {
    *exitRenderLoop = true;
    // Poll for a new instance.
    *requestRestart = true;
    break;
  }
  default:
    break;
  }
}

void OpenXrProgram::LogActionSourceName(XrAction action,
                                        const std::string &actionName) const {
  XrBoundSourcesForActionEnumerateInfo getInfo = {
      XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
  getInfo.action = action;
  uint32_t pathCount = 0;
  CHECK_XRCMD(xrEnumerateBoundSourcesForAction(m_session, &getInfo, 0,
                                               &pathCount, nullptr));
  std::vector<XrPath> paths(pathCount);
  CHECK_XRCMD(xrEnumerateBoundSourcesForAction(
      m_session, &getInfo, uint32_t(paths.size()), &pathCount, paths.data()));

  std::string sourceName;
  for (uint32_t i = 0; i < pathCount; ++i) {
    constexpr XrInputSourceLocalizedNameFlags all =
        XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
        XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
        XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;

    XrInputSourceLocalizedNameGetInfo nameInfo = {
        XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
    nameInfo.sourcePath = paths[i];
    nameInfo.whichComponents = all;

    uint32_t size = 0;
    CHECK_XRCMD(
        xrGetInputSourceLocalizedName(m_session, &nameInfo, 0, &size, nullptr));
    if (size < 1) {
      continue;
    }
    std::vector<char> grabSource(size);
    CHECK_XRCMD(xrGetInputSourceLocalizedName(m_session, &nameInfo,
                                              uint32_t(grabSource.size()),
                                              &size, grabSource.data()));
    if (!sourceName.empty()) {
      sourceName += " and ";
    }
    sourceName += "'";
    sourceName += std::string(grabSource.data(), size - 1);
    sourceName += "'";
  }

  Log::Write(Log::Level::Info,
             Fmt("%s action is bound to %s", actionName.c_str(),
                 ((!sourceName.empty()) ? sourceName.c_str() : "nothing")));
}

std::vector<XrSwapchainImageBaseHeader *>
OpenXrProgram::AllocateSwapchainImageStructs(
    uint32_t capacity, VkExtent2D size, VkFormat format,
    VkSampleCountFlagBits sampleCount,
    const std::shared_ptr<class VulkanGraphicsPlugin> &vulkan) {
  // Allocate and initialize the buffer of image structs (must be sequential
  // in memory for xrEnumerateSwapchainImages). Return back an array of
  // pointers to each swapchain image struct so the consumer doesn't need to
  // know the type/size. Keep the buffer alive by adding it into the list of
  // buffers.
  m_swapchainImageContexts.emplace_back(SwapchainImageContext::Create(
      vulkan->m_vkDevice, vulkan->m_memAllocator, capacity, size, format,
      sampleCount, vulkan->m_pipelineLayout, vulkan->m_shaderProgram,
      vulkan->m_drawBuffer));
  auto swapchainImageContext = m_swapchainImageContexts.back();

  // Map every swapchainImage base pointer to this context
  for (auto &base : swapchainImageContext->m_bases) {
    m_swapchainImageContextMap[base] = swapchainImageContext;
  }

  return swapchainImageContext->m_bases;
}

bool OpenXrProgram::LocateView(XrSession session, XrSpace appSpace,
                               XrTime predictedDisplayTime,
                               XrViewConfigurationType viewConfigType,
                               uint32_t *viewCountOutput) {
  XrViewLocateInfo viewLocateInfo{
      .type = XR_TYPE_VIEW_LOCATE_INFO,
      .viewConfigurationType = viewConfigType,
      .displayTime = predictedDisplayTime,
      .space = appSpace,
  };

  XrViewState viewState{
      .type = XR_TYPE_VIEW_STATE,
  };

  auto res = xrLocateViews(session, &viewLocateInfo, &viewState,
                           static_cast<uint32_t>(m_views.size()),
                           viewCountOutput, m_views.data());
  CHECK_XRRESULT(res, "xrLocateViews");
  if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
      (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
    return false; // There is no valid tracking poses for the views.
  }

  CHECK(*viewCountOutput == m_views.size());
  CHECK(*viewCountOutput == m_configViews.size());
  CHECK(*viewCountOutput == m_swapchains.size());

  return true;
}

OpenXrProgram::ViewSwapchainInfo
OpenXrProgram::AcquireSwapchainForView(uint32_t i) {
  // Each view has a separate swapchain which is acquired, rendered to, and
  // released.
  const Swapchain viewSwapchain = m_swapchains[i];

  XrSwapchainImageAcquireInfo acquireInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
  };
  uint32_t swapchainImageIndex;
  CHECK_XRCMD(xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo,
                                      &swapchainImageIndex));

  XrSwapchainImageWaitInfo waitInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
      .timeout = XR_INFINITE_DURATION,
  };
  CHECK_XRCMD(xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo));

  const XrSwapchainImageBaseHeader *const swapchainImage =
      m_swapchainImages[viewSwapchain.handle][swapchainImageIndex];

  ViewSwapchainInfo info = {};

  info.CompositionLayer = {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
      .pose = m_views[i].pose,
      .fov = m_views[i].fov,
      .subImage = {
          .swapchain = viewSwapchain.handle,
          .imageRect = {.offset = {0, 0}, .extent = viewSwapchain.extent}}};

  info.Swapchain = m_swapchainImageContextMap[swapchainImage];
  info.ImageIndex = info.Swapchain->ImageIndex(swapchainImage);

  return info;
}

void OpenXrProgram::EndSwapchain(XrSwapchain swapchain) {
  XrSwapchainImageReleaseInfo releaseInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
  };
  CHECK_XRCMD(xrReleaseSwapchainImage(swapchain, &releaseInfo));
}

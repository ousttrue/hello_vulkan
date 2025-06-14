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

#include "GetXrReferenceSpaceCreateInfo.h"
#include "openxr/openxr.h"
#include "openxr_program.h"
#include "openxr_session.h"
#include "options.h"
#include "xr_check.h"
#include <vko/vko.h>

#include <common/logger.h>
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

OpenXrProgram::VulkanResources OpenXrProgram::InitializeVulkan(
    const std::vector<const char *> &layers,
    const std::vector<const char *> &instanceExtensions,
    const std::vector<const char *> &deviceExtensions,
    const VkDebugUtilsMessengerCreateInfoEXT *debugInfo) {
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

  for (auto name : layers) {
    Log::Write(Log::Level::Info, Fmt("  vulkan layer: %s", name));
  }
  for (auto name : instanceExtensions) {
    Log::Write(Log::Level::Info, Fmt("  vulkan instance extensions: %s", name));
  }
  for (auto name : deviceExtensions) {
    Log::Write(Log::Level::Info, Fmt("  vulkan device extensions: %s", name));
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

  auto vkCreateDebugUtilsMessengerEXT =
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          vkInstance, "vkCreateDebugUtilsMessengerEXT");
  if (vkCreateDebugUtilsMessengerEXT != nullptr) {
    if (vkCreateDebugUtilsMessengerEXT(vkInstance, debugInfo, nullptr,
                                       &m_vkDebugUtilsMessenger) !=
        VK_SUCCESS) {
      throw std::runtime_error("vkCreateDebugUtilsMessengerEXT");
    }
  }

  g_vkSetDebugUtilsObjectNameEXT(vkInstance);

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

  for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    // Only need graphics (not presentation) for draw queue
    if ((queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
      queueInfo.queueFamilyIndex = i;
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

  return {
      .Instance = vkInstance,
      .PhysicalDevice = vkPhysicalDevice,
      .Device = vkDevice,
      .QueueFamilyIndex = queueInfo.queueFamilyIndex,
  };
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

std::shared_ptr<OpenXrSession>
OpenXrProgram::InitializeSession(VulkanResources vulkan) {
  CHECK(m_instance != XR_NULL_HANDLE);

  XrSession session;
  {
    Log::Write(Log::Level::Verbose, Fmt("Creating session..."));

    XrGraphicsBindingVulkan2KHR graphicsBinding{
        .type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR,
        .next = nullptr,
        .instance = vulkan.Instance,
        .physicalDevice = vulkan.PhysicalDevice,
        .device = vulkan.Device,
        .queueFamilyIndex = vulkan.QueueFamilyIndex,
        .queueIndex = 0,
    };
    XrSessionCreateInfo createInfo{
        .type = XR_TYPE_SESSION_CREATE_INFO,
        .next = &graphicsBinding,
        .systemId = m_systemId,
    };
    CHECK_XRCMD(xrCreateSession(m_instance, &createInfo, &session));
  }

  LogReferenceSpaces(session);

  XrSpace appSpace;
  {
    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo =
        GetXrReferenceSpaceCreateInfo(m_options.AppSpace);
    CHECK_XRCMD(
        xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &appSpace));
  }

  auto ptr = std::shared_ptr<OpenXrSession>(
      new OpenXrSession(m_options, m_instance, m_systemId, session, appSpace));
  ptr->InitializeActions();
  ptr->CreateVisualizedSpaces();
  return ptr;
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

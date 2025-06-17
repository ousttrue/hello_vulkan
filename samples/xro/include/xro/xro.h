#pragma once
#include <vko/vko.h>
#ifdef XR_USE_PLATFORM_ANDROID
#include <android_native_app_glue.h>
#endif
#ifdef XR_USE_PLATFORM_WIN32
#include <Unknwn.h>
#endif
#include <algorithm>
#include <openxr/openxr_platform.h>
#include <vector>

#define XRO_CHECK(cmd) xro::CheckXrResult(cmd, #cmd, VKO_FILE_AND_LINE);
// #define CHECK_XRRESULT(res, cmdStr) CheckXrResult(res, cmdStr,
// VKO_FILE_AND_LINE);
#define XRO_THROW(xr, cmd) xro::ThrowXrResult(xr, #cmd, VKO_FILE_AND_LINE);

namespace xro {

using Logger = vko::Logger;

[[noreturn]] inline void ThrowXrResult(XrResult res,
                                       const char *originator = nullptr,
                                       const char *sourceLocation = nullptr) {
  vko::Throw(vko::fmt("XrResult failure [%d]", res), originator,
             sourceLocation);
}

inline XrResult CheckXrResult(XrResult res, const char *originator = nullptr,
                              const char *sourceLocation = nullptr) {
  if (XR_FAILED(res)) {
    xro::ThrowXrResult(res, originator, sourceLocation);
  }

  return res;
}

struct Instance : public vko::not_copyable {
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
      Logger::Info("xro::Instance::~Instance ...");
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

    // Read graphics properties for preferred swapchain length and logging.
    XrSystemProperties systemProperties{
        .type = XR_TYPE_SYSTEM_PROPERTIES,
    };
    XRO_CHECK(xrGetSystemProperties(this->instance, this->systemId,
                                    &systemProperties));

    // Log system properties.
    Logger::Info("System Properties: Name=%s VendorId=%d",
                 systemProperties.systemName, systemProperties.vendorId);
    Logger::Info(
        "System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
        systemProperties.graphicsProperties.maxSwapchainImageWidth,
        systemProperties.graphicsProperties.maxSwapchainImageHeight,
        systemProperties.graphicsProperties.maxLayerCount);
    Logger::Info("System Tracking Properties: OrientationTracking=%s "
                 "PositionTracking=%s",
                 systemProperties.trackingProperties.orientationTracking ==
                         XR_TRUE
                     ? "True"
                     : "False",
                 systemProperties.trackingProperties.positionTracking == XR_TRUE
                     ? "True"
                     : "False");

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

  std::tuple<vko::Instance, vko::PhysicalDevice, vko::Device>
  createVulkan(PFN_vkDebugUtilsMessengerCallbackEXT pfnUserCallback) {
    vko::Instance instance;
    instance.appInfo.pApplicationName = "hello_xr";
    instance.appInfo.pEngineName = "hello_xr";
    instance.debugUtilsMessengerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = pfnUserCallback,
        .pUserData = nullptr,
    };
    vko::Device device;

#ifdef NDEBUG
#else
    instance.debugUtilsMessengerCreateInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    instance.pushFirstSupportedValidationLayer({
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_standard_validation",
    });
    instance.pushFirstSupportedInstanceExtension({
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    });
#endif

    // Create the Vulkan device for the adapter associated with the system.
    // Extension function must be loaded by name
    XrGraphicsRequirementsVulkan2KHR graphicsRequirements{
        .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR,
    };
    XRO_CHECK(GetVulkanGraphicsRequirements2KHR(this->instance, this->systemId,
                                                &graphicsRequirements));

    for (auto name : instance.validationLayers) {
      Logger::Info("  vulkan layer: %s", name);
    }
    for (auto name : instance.instanceExtensions) {
      Logger::Info("  vulkan instance extensions: %s", name);
    }
    for (auto name : device.deviceExtensions) {
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
        .enabledLayerCount =
            static_cast<uint32_t>(instance.validationLayers.size()),
        .ppEnabledLayerNames = instance.validationLayers.data(),
        .enabledExtensionCount =
            static_cast<uint32_t>(instance.instanceExtensions.size()),
        .ppEnabledExtensionNames = instance.instanceExtensions.data(),
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
    instance.reset(vkInstance);

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
        .enabledLayerCount =
            static_cast<uint32_t>(instance.validationLayers.size()),
        .ppEnabledLayerNames = instance.validationLayers.data(),
        .enabledExtensionCount =
            static_cast<uint32_t>(device.deviceExtensions.size()),
        .ppEnabledExtensionNames = device.deviceExtensions.data(),
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
    device.reset(vkDevice);
    vko::VKO_CHECK(err);

    return {std::move(instance), vko::PhysicalDevice(vkPhysicalDevice),
            std::move(device)};
  }
};

struct Session : public vko::not_copyable {
  XrSession session;
  operator XrSession() const { return this->session; }
  std::vector<VkFormat> formats;
  Session(XrInstance _instance, XrSystemId _systemId, VkInstance instance,
          VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex,
          VkDevice device) {
    // Log::Write(Log::Level::Verbose, Fmt("Creating session..."));
    XrGraphicsBindingVulkan2KHR graphicsBinding{
        .type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR,
        .next = nullptr,
        .instance = instance,
        .physicalDevice = physicalDevice,
        .device = device,
        .queueFamilyIndex = queueFamilyIndex,
        .queueIndex = 0,
    };
    XrSessionCreateInfo createInfo{
        .type = XR_TYPE_SESSION_CREATE_INFO,
        .next = &graphicsBinding,
        .systemId = _systemId,
    };
    XRO_CHECK(xrCreateSession(_instance, &createInfo, &this->session));

    // Select a swapchain format.
    uint32_t swapchainFormatCount;
    XRO_CHECK(xrEnumerateSwapchainFormats(this->session, 0,
                                          &swapchainFormatCount, nullptr));
    this->formats.resize(swapchainFormatCount);
    XRO_CHECK(xrEnumerateSwapchainFormats(this->session, swapchainFormatCount,
                                          &swapchainFormatCount,
                                          (int64_t *)formats.data()));
  }
  ~Session() {
    Logger::Info("xro::Session::~Session ...");
    xrEndSession(this->session);
    xrDestroySession(this->session);
  }

  VkFormat selectColorSwapchainFormat() const {
    // List of supported color swapchain formats.
    constexpr VkFormat SupportedColorSwapchainFormats[] = {
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
    };

    auto swapchainFormatIt =
        std::find_first_of(formats.begin(), formats.end(),
                           std::begin(SupportedColorSwapchainFormats),
                           std::end(SupportedColorSwapchainFormats));
    if (swapchainFormatIt == formats.end()) {
      throw std::runtime_error(
          "No runtime swapchain format supported for color swapchain");
    }

    // Print swapchain formats and the selected one.
    {
      std::string swapchainFormatsString;
      for (auto format : this->formats) {
        const bool selected = format == *swapchainFormatIt;
        swapchainFormatsString += " ";
        if (selected) {
          swapchainFormatsString += "[";
        }
        swapchainFormatsString += std::to_string(format);
        if (selected) {
          swapchainFormatsString += "]";
        }
      }
      Logger::Info("Swapchain Formats: %s", swapchainFormatsString.c_str());
    }
    return *swapchainFormatIt;
  }
};

inline XrFrameState beginFrame(XrSession session) {
  XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
  XrFrameState frameState{XR_TYPE_FRAME_STATE};
  XRO_CHECK(xrWaitFrame(session, &frameWaitInfo, &frameState));
  XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
  XRO_CHECK(xrBeginFrame(session, &frameBeginInfo));
  return frameState;
}

inline void
endFrame(XrSession session, XrTime predictedDisplayTime,
         XrEnvironmentBlendMode blendMode,
         const std::vector<XrCompositionLayerBaseHeader *> &layers) {
  XrFrameEndInfo frameEndInfo{
      .type = XR_TYPE_FRAME_END_INFO,
      .displayTime = predictedDisplayTime,
      .environmentBlendMode = blendMode,
      .layerCount = static_cast<uint32_t>(layers.size()),
      .layers = layers.data(),
  };
  XRO_CHECK(xrEndFrame(session, &frameEndInfo));
}

struct Stereoscope {
  std::vector<XrViewConfigurationView> viewConfigurations;
  std::vector<XrView> views;

  Stereoscope(XrInstance instance, XrSystemId systemId,
              XrViewConfigurationType viewConfigurationType) {
    // Query and cache view configuration views.
    uint32_t viewCount;
    XRO_CHECK(xrEnumerateViewConfigurationViews(
        instance, systemId, viewConfigurationType, 0, &viewCount, nullptr));
    this->viewConfigurations.resize(viewCount,
                                    {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    this->views.resize(viewCount, {XR_TYPE_VIEW});
    XRO_CHECK(xrEnumerateViewConfigurationViews(
        instance, systemId, viewConfigurationType, viewCount, &viewCount,
        viewConfigurations.data()));
  }

  bool Locate(XrSession session, XrSpace appSpace, XrTime predictedDisplayTime,
              XrViewConfigurationType viewConfigType) {
    XrViewLocateInfo viewLocateInfo{
        .type = XR_TYPE_VIEW_LOCATE_INFO,
        .viewConfigurationType = viewConfigType,
        .displayTime = predictedDisplayTime,
        .space = appSpace,
    };

    XrViewState viewState{
        .type = XR_TYPE_VIEW_STATE,
    };

    uint32_t viewCountOutput;
    auto res = xrLocateViews(session, &viewLocateInfo, &viewState,
                             static_cast<uint32_t>(this->views.size()),
                             &viewCountOutput, this->views.data());
    // CHECK_XRRESULT(res, "xrLocateViews");
    if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
        (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
      return false; // There is no valid tracking poses for the views.
    }

    // CHECK(*viewCountOutput == this->views.size());
    // CHECK(*viewCountOutput == m_configViews.size());
    // CHECK(*viewCountOutput == m_swapchains.size());

    return true;
  }
};

inline std::tuple<XrResult, XrSpaceLocation>
locate(XrSpace world, XrTime predictedDisplayTime, XrSpace target) {
  XrSpaceLocation spaceLocation{
      .type = XR_TYPE_SPACE_LOCATION,
  };
  auto res = xrLocateSpace(target, world, predictedDisplayTime, &spaceLocation);
  return {res, spaceLocation};
}

inline bool locationIsValid(XrResult res, const XrSpaceLocation &location) {
  if (!XR_UNQUALIFIED_SUCCESS(res)) {
    return false;
  }
  if ((location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) == 0) {
    return false;
  }
  if ((location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) == 0) {
    return false;
  }
  return true;
}

class SessionState {
  XrInstance m_instance;
  XrSession m_session;
  XrViewConfigurationType m_viewConfigurationType;

  // Application's current lifecycle state according to the runtime
  XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;
  bool m_sessionRunning = false;
  XrEventDataBuffer m_eventDataBuffer;

public:
  SessionState(XrInstance instance, XrSession session,
               XrViewConfigurationType viewConfigurationType)
      : m_instance(instance), m_session(session),
        m_viewConfigurationType(viewConfigurationType) {}

  // Process any events in the event queue.
  struct PollResult {
    bool exitRenderLoop;
    bool requestRestart;
    bool isSessionRunning;
  };
  PollResult PollEvents() {
    PollResult res{
        .exitRenderLoop = false,
        .requestRestart = false,
        .isSessionRunning = m_sessionRunning,
    };

    // Process all pending messages.
    while (const XrEventDataBaseHeader *event = TryReadNextEvent()) {
      switch (event->type) {
      case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
        const auto &instanceLossPending =
            *reinterpret_cast<const XrEventDataInstanceLossPending *>(event);
        xro::Logger::Error("XrEventDataInstanceLossPending by %lld",
                           instanceLossPending.lossTime);
        return {true, true, m_sessionRunning};
      }

      case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
        auto sessionStateChangedEvent =
            *reinterpret_cast<const XrEventDataSessionStateChanged *>(event);
        HandleSessionStateChangedEvent(
            sessionStateChangedEvent, &res.exitRenderLoop, &res.requestRestart);
        break;
      }

      case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
        // m_input.Log(m_session);
        break;

      // case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
      default: {
        xro::Logger::Info("Ignoring event type %d", event->type);
        break;
      }
      }
    }

    return res;
  }

private:
  // Return event if one is available, otherwise return null.
  const XrEventDataBaseHeader *TryReadNextEvent() {
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
        xro::Logger::Error("%d events lost", eventsLost->lostEventCount);
      }

      return baseHeader;
    }
    if (xr == XR_EVENT_UNAVAILABLE) {
      return nullptr;
    }
    XRO_THROW(xr, "xrPollEvent");
  }
  void HandleSessionStateChangedEvent(
      const XrEventDataSessionStateChanged &stateChangedEvent,
      bool *exitRenderLoop, bool *requestRestart) {
    const XrSessionState oldState = m_sessionState;
    m_sessionState = stateChangedEvent.state;

    // xro::Logger::Info("XrEventDataSessionStateChanged: state %s->%s
    // session=%lld "
    //                   "time=%lld",
    //                   to_string(oldState), to_string(m_sessionState),
    //                   stateChangedEvent.session, stateChangedEvent.time);

    if ((stateChangedEvent.session != XR_NULL_HANDLE) &&
        (stateChangedEvent.session != m_session)) {
      xro::Logger::Error("XrEventDataSessionStateChanged for unknown session");
      return;
    }

    switch (m_sessionState) {
    case XR_SESSION_STATE_READY: {
      VKO_ASSERT(m_session != XR_NULL_HANDLE);
      XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
      sessionBeginInfo.primaryViewConfigurationType = m_viewConfigurationType;
      XRO_CHECK(xrBeginSession(m_session, &sessionBeginInfo));
      m_sessionRunning = true;
      break;
    }
    case XR_SESSION_STATE_STOPPING: {
      VKO_ASSERT(m_session != XR_NULL_HANDLE);
      m_sessionRunning = false;
      XRO_CHECK(xrEndSession(m_session))
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
};

} // namespace xro

#pragma once
#include "../vuloxr.h"
#include <assert.h>
#include <chrono>
#include <climits>
#include <cstdint>
#include <list>
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

struct Platform : NonCopyable {
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

inline VkDeviceMemory createBindMemory(VkDevice device, uint32_t reqSize,
                                       uint32_t memoryTypeIndex) {
  VkMemoryAllocateInfo alloc = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = reqSize,
      .memoryTypeIndex = memoryTypeIndex,
  };
  VkDeviceMemory memory;
  vuloxr::vk::CheckVkResult(vkAllocateMemory(device, &alloc, nullptr, &memory));
  return memory;
}

struct PhysicalDevice {
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  operator VkPhysicalDevice() const { return this->physicalDevice; }
  std::vector<VkExtensionProperties> deviceExtensionProperties;

  VkPhysicalDeviceProperties properties = {};
  VkPhysicalDeviceFeatures features = {};
  std::vector<VkQueueFamilyProperties> queueFamilyProperties;
  uint32_t graphicsFamilyIndex = UINT_MAX;
  VkPhysicalDeviceMemoryProperties memoryProps = {};

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

    vkGetPhysicalDeviceMemoryProperties(this->physicalDevice,
                                        &this->memoryProps);
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

  struct SizeAndTypeIndex {
    VkDeviceSize requiredSize;
    uint32_t memoryTypeIndex;
  };

  SizeAndTypeIndex
  findMemoryTypeIndex(VkDevice device,
                      const VkMemoryRequirements &memoryRequirements,
                      uint32_t hostRequirements) const {
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
      if (memoryRequirements.memoryTypeBits & (1u << i)) {
        if ((this->memoryProps.memoryTypes[i].propertyFlags &
             hostRequirements)) {
          return {
              .requiredSize = memoryRequirements.size,
              .memoryTypeIndex = i,
          };
        }
      }
    }
    vuloxr::Logger::Error("Failed to obtain suitable memory type.\n");
    abort();
  }

  VkDeviceMemory allocAndMapMemoryForBuffer(VkDevice device, VkBuffer buffer,
                                            const void *src,
                                            uint32_t srcSize) const {
    // alloc
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
    auto [requiredSize, typeIndex] =
        this->findMemoryTypeIndex(device, memoryRequirements,
                                  // for map
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    assert(requiredSize >= srcSize);
    auto memory = createBindMemory(device, requiredSize, typeIndex);
    // bind
    vkBindBufferMemory(device, buffer, memory, 0);
    // map & copy
    {
      void *dst;
      CheckVkResult(vkMapMemory(device, memory, 0, srcSize, 0, &dst));
      memcpy(dst, src, srcSize);
      vkUnmapMemory(device, memory);
    }
    return memory;
  }

  VkDeviceMemory allocMemoryForImage(VkDevice device, VkImage image) const {
    // alloc
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(device, image, &memoryRequirements);
    auto [requiredSize, typeIndex] =
        this->findMemoryTypeIndex(device, memoryRequirements,
                                  // for copy command
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    auto memory = createBindMemory(device, requiredSize, typeIndex);
    // bind
    vkBindImageMemory(device, image, memory, 0);
    return memory;
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

  VkApplicationInfo appInfo{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "VULOXR",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };

  VkInstanceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &this->appInfo,
  };
  VkInstance instance = VK_NULL_HANDLE;
  operator VkInstance() const { return this->instance; }
  std::vector<PhysicalDevice> physicalDevices;

  // VK_EXT_DEBUG_UTILS_EXTENSION_NAME
  VkDebugUtilsMessengerEXT debugUtilsMessenger = VK_NULL_HANDLE;
  VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
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
            if (message_severity &
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
              Logger::Error("%s %s\n", callback_data->pMessageIdName,
                            callback_data->pMessage);
            } else if (message_severity &
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
              Logger::Warn("%s %s\n", callback_data->pMessageIdName,
                           callback_data->pMessage);
            } else if (message_severity &
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
              Logger::Info("%s %s\n", callback_data->pMessageIdName,
                           callback_data->pMessage);
            } else /*if(message_severity &
                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)*/
            {
              Logger::Verbose("%s %s\n", callback_data->pMessageIdName,
                              callback_data->pMessage);
            }

            // }
            return VK_FALSE;
          },
  };
  static VkResult CreateDebugUtilsMessengerEXT(
      VkInstance instance,
      const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
      const VkAllocationCallbacks *pAllocator,
      VkDebugUtilsMessengerEXT *pDebugMessenger) {
    Logger::Info(VK_EXT_DEBUG_UTILS_EXTENSION_NAME
                 " => vkCreateDebugUtilsMessengerEXT");
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
    Logger::Error("[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n",
                  objectType, pMessage);
    return VK_FALSE;
  }
  static VkResult
  CreateDebugReportCallbackEXT(VkInstance instance,
                               const VkDebugReportCallbackCreateInfoEXT *ci,
                               VkDebugReportCallbackEXT *debugReport) {
    Logger::Info("VK_EXT_debug_report => vkCreateDebugReportCallbackEXT()");
    auto f_vkCreateDebugReportCallbackEXT =
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
            instance, "vkCreateDebugReportCallbackEXT");
    assert(f_vkCreateDebugReportCallbackEXT);
    return f_vkCreateDebugReportCallbackEXT(
        instance, ci, nullptr /*g_Allocator*/, debugReport);
  }
  static void
  DestroyDebugReportCallbackEXT(VkInstance instance,
                                VkDebugReportCallbackEXT debugReport) {
    // Remove the debug report callback
    auto f_vkDestroyDebugReportCallbackEXT =
        (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
            instance, "vkDestroyDebugReportCallbackEXT");
    f_vkDestroyDebugReportCallbackEXT(instance, debugReport, nullptr);
  }

  Instance() {
    // https://developer.android.com/games/develop/vulkan/native-engine-support
    uint32_t version;
    CheckVkResult(vkEnumerateInstanceVersion(&version));
    if (version >= VK_API_VERSION_1_3) {
      Logger::Info("VULKAN_1_3");
      this->appInfo.apiVersion = VK_API_VERSION_1_3;
    } else if (version >= VK_API_VERSION_1_2) {
      Logger::Info("VULKAN_1_2");
      this->appInfo.apiVersion = VK_API_VERSION_1_2;
    } else if (version >= VK_API_VERSION_1_1) {
      Logger::Info("VULKAN_1_1");
      this->appInfo.apiVersion = VK_API_VERSION_1_1;
    } else {
      Logger::Info("VULKAN_1_0");
      this->appInfo.apiVersion = VK_API_VERSION_1_0;
    }
  }
  ~Instance() {
    if (this->debugReport != VK_NULL_HANDLE) {
      DestroyDebugReportCallbackEXT(this->instance, this->debugReport);
    }
    if (this->debugUtilsMessenger != VK_NULL_HANDLE) {
      DestroyDebugUtilsMessengerEXT(this->instance, this->debugUtilsMessenger,
                                    nullptr);
    }
    if (this->instance != VK_NULL_HANDLE) {
      Logger::Info("Instance::~Instance: 0x%x", this->instance);
      vkDestroyInstance(instance, nullptr);
    }
  }
  Instance(Instance &&rhs) {
    this->debugUtilsMessenger = rhs.debugUtilsMessenger;
    rhs.debugUtilsMessenger = VK_NULL_HANDLE;
    this->debugReport = rhs.debugReport;
    rhs.debugReport = VK_NULL_HANDLE;
    this->reset(rhs.instance);
    rhs.instance = VK_NULL_HANDLE;
  }
  Instance &operator=(Instance &&rhs) {
    this->debugUtilsMessenger = rhs.debugUtilsMessenger;
    rhs.debugUtilsMessenger = VK_NULL_HANDLE;
    this->debugReport = rhs.debugReport;
    rhs.debugReport = VK_NULL_HANDLE;
    this->reset(rhs.instance);
    rhs.instance = VK_NULL_HANDLE;
    return *this;
  }

  VkResult create() {
    addExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    for (auto name : this->layers) {
      Logger::Info("instance layer: %s", name);
    }
    for (auto name : this->extensions) {
      Logger::Info("instance extension: %s", name);
      if (strcmp(name, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
        this->create_info.pNext = &this->debugUtilsMessengerCreateInfo;
      }
    }
    this->create_info.enabledLayerCount =
        static_cast<uint32_t>(std::size(this->layers));
    this->create_info.ppEnabledLayerNames = this->layers.data();
    this->create_info.enabledExtensionCount =
        static_cast<uint32_t>(std::size(this->extensions));
    this->create_info.ppEnabledExtensionNames = this->extensions.data();

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

    // extensions
    if (find_name(this->extensions, "VK_EXT_debug_report") &&
        this->debugReport == VK_NULL_HANDLE) {
      CheckVkResult(CreateDebugReportCallbackEXT(
          this->instance, &this->debug_report_ci, &this->debugReport));
    }
    if (find_name(this->extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) &&
        this->debugUtilsMessenger == VK_NULL_HANDLE) {
      CheckVkResult(CreateDebugUtilsMessengerEXT(
          this->instance, &this->debugUtilsMessengerCreateInfo, nullptr,
          &this->debugUtilsMessenger));
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
      Logger::Info("Device::~Device: 0x%x", this->device);
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
  VkResult create(VkInstance instance, const VkPhysicalDevice physicalDevice,
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

    for (auto name : this->extensions) {
      Logger::Info("device extension: %s", name);
    }
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
    VkPipelineStageFlags waitDstStageMask =
        VK_PIPELINE_STAGE_TRANSFER_BIT |
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
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
  uint32_t graphicsFamily;

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
    this->createInfo.minImageCount =
        this->surfaceCapabilities.minImageCount + 1;
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
    Logger::Verbose("frames in flight: %d", flightCount);
    VkCommandPoolCreateInfo commandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsQueueIndex,
    };
    CheckVkResult(vkCreateCommandPool(this->device, &commandPoolCreateInfo,
                                      nullptr, &this->pool));

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = this->pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = flightCount,
    };
    CheckVkResult(vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo,
                                           this->commandBuffers.data()));

    for (auto &flight : this->flights) {
      VkFenceCreateInfo fenceInfo = {
          .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
          .flags = VK_FENCE_CREATE_SIGNALED_BIT,
      };
      CheckVkResult(vkCreateFence(this->device, &fenceInfo, nullptr,
                                  &flight.submitFence));

      VkSemaphoreCreateInfo semaphoreInfo = {
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      };
      CheckVkResult(vkCreateSemaphore(this->device, &semaphoreInfo, nullptr,
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

    Logger::Verbose("* create acquireSemaphore *");
    VkSemaphore semaphore;
    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    CheckVkResult(
        vkCreateSemaphore(this->device, &semaphoreInfo, nullptr, &semaphore));
    this->acquireSemaphoresOwn.push_back(semaphore);
    return semaphore;
  }

  std::tuple<VkCommandBuffer, Flight> sync(VkSemaphore acquireSemaphore) {
    auto index = (this->frameCount++) % this->flights.size();
    // keep acquireSemaphore
    auto &flight = this->flights[index];
    auto oldSemaphore = flight.acquireSemaphore;
    if (oldSemaphore != VK_NULL_HANDLE) {
      this->reuseSemaphore(oldSemaphore);
    }
    flight.acquireSemaphore = acquireSemaphore;

    // block. ensure oldSemaphore be signaled.
    vkWaitForFences(this->device, 1, &flight.submitFence, true, UINT64_MAX);
    vkResetFences(this->device, 1, &flight.submitFence);

    auto cmd = this->commandBuffers[index];

    return {cmd, flight};
  }

  void reuseSemaphore(VkSemaphore semaphore) {
    this->acquireSemaphoresReuse.push_back(semaphore);
  }
};

struct SwapchainFramebuffer {
  VkDevice device;

  VkImageViewCreateInfo imageViewCreateInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .a = VK_COMPONENT_SWIZZLE_IDENTITY},
      //     colorViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
      //     colorViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
      //     colorViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
      //     colorViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
  };
  VkImageView imageView = VK_NULL_HANDLE;

  VkImageViewCreateInfo depthViewCreateInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .a = VK_COMPONENT_SWIZZLE_IDENTITY},
      //     depthViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
      //     depthViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
      //     depthViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
      //     depthViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
  };
  VkImageView depthView = VK_NULL_HANDLE;

  VkFramebuffer framebuffer = VK_NULL_HANDLE;

  SwapchainFramebuffer(VkDevice _device, VkImage image, VkExtent2D extent,
                       VkFormat format, VkRenderPass renderPass,
                       VkImage depth = {}, VkFormat depthFormat = {})
      : device(_device) {

    // imageView
    this->imageViewCreateInfo.image = image;
    this->imageViewCreateInfo.format = format;
    vuloxr::vk::CheckVkResult(vkCreateImageView(
        this->device, &this->imageViewCreateInfo, nullptr, &this->imageView));

    if (depth) {
      // depthView
      this->depthViewCreateInfo.image = depth;
      this->depthViewCreateInfo.format = depthFormat;
      vuloxr::vk::CheckVkResult(vkCreateImageView(
          this->device, &this->depthViewCreateInfo, nullptr, &this->depthView));

      VkImageView attachments[] = {this->imageView, this->depthView};
      VkFramebufferCreateInfo framebufferInfo{
          .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
          .renderPass = renderPass,
          .attachmentCount = 2,
          .pAttachments = attachments,
          .width = extent.width,
          .height = extent.height,
          .layers = 1,
      };
      vuloxr::vk::CheckVkResult(vkCreateFramebuffer(
          this->device, &framebufferInfo, nullptr, &this->framebuffer));
    } else {
      VkFramebufferCreateInfo framebufferInfo{
          .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
          .renderPass = renderPass,
          .attachmentCount = 1,
          .pAttachments = &this->imageView,
          .width = extent.width,
          .height = extent.height,
          .layers = 1,
      };
      vuloxr::vk::CheckVkResult(vkCreateFramebuffer(
          device, &framebufferInfo, nullptr, &this->framebuffer));
    }
  }

  ~SwapchainFramebuffer() {
    vkDestroyFramebuffer(this->device, this->framebuffer, nullptr);
    vkDestroyImageView(this->device, this->imageView, nullptr);
    if (this->depthView != VK_NULL_HANDLE) {
      vkDestroyImageView(this->device, depthView, nullptr);
    }
  }
};

} // namespace vk
} // namespace vuloxr

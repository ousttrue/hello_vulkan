#pragma once
#include "../vuloxr.h"
#include <assert.h>
#include <chrono>
#include <climits>
#include <cstdint>
#include <list>
#include <magic_enum/magic_enum.hpp>
#include <span>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace vuloxr {

namespace vk {

inline std::tuple<int, int, int> extract_version(uint32_t version) {
  auto major = version >> 22;
  auto minor = (version >> 12) & 0x3ff;
  auto patch = version & 0xfff;
  return {major, minor, patch};
}

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

struct Memory : NonCopyable {
  VkDevice device = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  Memory() = default;
  Memory(VkDevice _device, VkDeviceMemory _memory = VK_NULL_HANDLE)
      : device(_device), memory(_memory) {}
  ~Memory() { release(); }
  Memory(Memory &&rhs) {
    release();
    this->memory = rhs.memory;
    rhs.memory = VK_NULL_HANDLE;
    this->device = rhs.device;
  }
  Memory &operator=(Memory &&rhs) {
    release();
    this->memory = rhs.memory;
    rhs.memory = VK_NULL_HANDLE;
    this->device = rhs.device;
    return *this;
  }
  void mapWrite(const void *src, uint32_t srcSize) const {
    void *dst;
    CheckVkResult(vkMapMemory(device, memory, 0, srcSize, 0, &dst));
    memcpy(dst, src, srcSize);
    vkUnmapMemory(device, memory);
  }

private:
  void release() {
    if (this->memory != VK_NULL_HANDLE) {
      vkFreeMemory(this->device, this->memory, nullptr);
      this->memory = VK_NULL_HANDLE;
    }
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

  Memory allocForMap(VkDevice device, VkBuffer buffer) const {
    // alloc
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
    auto [requiredSize, typeIndex] =
        this->findMemoryTypeIndex(device, memoryRequirements,
                                  // for map
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    auto memory = createBindMemory(device, requiredSize, typeIndex);
    // bind
    vkBindBufferMemory(device, buffer, memory, 0);
    return {device, memory};
  }

  Memory allocForTransfer(VkDevice device, VkBuffer buffer) const {
    // alloc
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
    auto [requiredSize, typeIndex] =
        this->findMemoryTypeIndex(device, memoryRequirements,
                                  // for map
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    auto memory = createBindMemory(device, requiredSize, typeIndex);
    // bind
    vkBindBufferMemory(device, buffer, memory, 0);
    return {device, memory};
  }

  Memory allocForTransfer(VkDevice device, VkImage image) const {
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
    return {device, memory};
  }

  VkFormat depthFormat() const {
    /* allow custom depth formats */
#ifdef __ANDROID__
    // Depth format needs to be VK_FORMAT_D24_UNORM_S8_UINT on
    // Android (if available).
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(this->physicalDevice,
                                        VK_FORMAT_D24_UNORM_S8_UINT, &props);
    if ((props.linearTilingFeatures &
         VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ||
        (props.optimalTilingFeatures &
         VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
      return VK_FORMAT_D24_UNORM_S8_UINT;
    else
      return VK_FORMAT_D16_UNORM;
#elif defined(VK_USE_PLATFORM_IOS_MVK)
    return VK_FORMAT_D32_SFLOAT;
#else
    return VK_FORMAT_D16_UNORM;
#endif
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
              Logger::Error("%s %s", callback_data->pMessageIdName,
                            callback_data->pMessage);
            } else if (message_severity &
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
              Logger::Warn("%s %s", callback_data->pMessageIdName,
                           callback_data->pMessage);
            } else if (message_severity &
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
              Logger::Info("%s %s", callback_data->pMessageIdName,
                           callback_data->pMessage);
            } else /*if(message_severity &
                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)*/
            {
              Logger::Verbose("%s %s", callback_data->pMessageIdName,
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

  VkResult submit(VkCommandBuffer cmd,
                  VkSemaphore acquireSemaphore = VK_NULL_HANDLE,
                  VkSemaphore submitSemaphore = VK_NULL_HANDLE,
                  VkFence submitFence = VK_NULL_HANDLE) const {
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 0,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };

    VkPipelineStageFlags waitDstStageMask =
        VK_PIPELINE_STAGE_TRANSFER_BIT |
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // VkPipelineStageFlags pipe_stage_flags =
    // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    if (acquireSemaphore != VK_NULL_HANDLE) {
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.pWaitSemaphores = &acquireSemaphore;
      submitInfo.pWaitDstStageMask = &waitDstStageMask;
    }

    if (submitSemaphore != VK_NULL_HANDLE) {
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pSignalSemaphores = &submitSemaphore;
    }

    return vkQueueSubmit(this->queue, 1, &submitInfo, submitFence);
  }
};

struct CommandBufferPool : NonCopyable {
  VkDevice device;
  VkCommandPool pool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> commandBuffers;

  CommandBufferPool(
      VkDevice _device, uint32_t graphicsQueueIndex, uint32_t poolSize,
      uint32_t flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                       VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
      : device(_device), commandBuffers(poolSize) {
    VkCommandPoolCreateInfo commandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = flags,
        .queueFamilyIndex = graphicsQueueIndex,
    };
    CheckVkResult(vkCreateCommandPool(this->device, &commandPoolCreateInfo,
                                      nullptr, &this->pool));

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = this->pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = poolSize,
    };
    CheckVkResult(vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo,
                                           this->commandBuffers.data()));
  }

  ~CommandBufferPool() {
    if (this->commandBuffers.size()) {
      vkFreeCommandBuffers(this->device, this->pool,
                           this->commandBuffers.size(),
                           this->commandBuffers.data());
    }
    vkDestroyCommandPool(this->device, this->pool, nullptr);
  }
};

struct Fence : NonCopyable {
  VkDevice device = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  operator VkFence() const { return this->fence; }
  Fence() {}
  Fence(VkDevice _device, bool signaled) : device(_device) {
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    if (signaled) {
      fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }
    CheckVkResult(
        vkCreateFence(this->device, &fenceInfo, nullptr, &this->fence));
  }
  ~Fence() {
    if (this->fence != VK_NULL_HANDLE) {
      vkDestroyFence(this->device, this->fence, nullptr);
    }
  }

  Fence &operator=(Fence &&rhs) {
    this->device = rhs.device;
    this->fence = rhs.fence;
    rhs.fence = VK_NULL_HANDLE;
    return *this;
  }

  void reset() { vkResetFences(this->device, 1, &this->fence); }

  void wait() {
    CheckVkResult(
        vkWaitForFences(this->device, 1, &this->fence, VK_TRUE, UINT64_MAX));
  }
};

struct AcquireSemaphorePool : NonCopyable {
  VkDevice device;

  std::list<VkSemaphore> semaphores;
  std::list<VkSemaphore> reusable;
  std::unordered_map<VkFence, VkSemaphore> fenceSemaphoreMap;

  AcquireSemaphorePool(VkDevice _device) : device(_device) {}

  ~AcquireSemaphorePool() {
    for (auto semaphore : this->semaphores) {
      vkDestroySemaphore(this->device, semaphore, nullptr);
    }
  }

  VkSemaphore getOrCreate() {
    if (!this->reusable.empty()) {
      auto semaphore = this->reusable.front();
      this->reusable.pop_front();
      return semaphore;
    }

    Logger::Verbose("* create acquireSemaphore *");
    VkSemaphore semaphore;
    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    CheckVkResult(
        vkCreateSemaphore(this->device, &semaphoreInfo, nullptr, &semaphore));
    this->semaphores.push_back(semaphore);
    return semaphore;
  }

  void reuse(VkSemaphore acquireSemaphore) {
    this->reusable.push_back(acquireSemaphore);
  }

  // wait, reset old fence
  // reuse combined old acquireSemaphore
  // new pair fence and acquireSemaphore
  void resetFenceAndMakePairSemaphore(VkFence submitFence,
                                      VkSemaphore acquireSemaphore) {
    // block. ensure oldSemaphore be signaled.
    vkWaitForFences(this->device, 1, &submitFence, true, UINT64_MAX);
    vkResetFences(this->device, 1, &submitFence);

    auto it = this->fenceSemaphoreMap.find(submitFence);
    if (it != this->fenceSemaphoreMap.end()) {
      reuse(it->second);
      this->fenceSemaphoreMap.erase(it);
    }

    this->fenceSemaphoreMap.insert({submitFence, acquireSemaphore});
  }
};

} // namespace vk
} // namespace vuloxr

#include "platform.hpp"
#include "common.hpp"
#include <string.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

using namespace std;

#ifdef FORCE_NO_VALIDATION
#define ENABLE_VALIDATION_LAYERS 0
#else
#define ENABLE_VALIDATION_LAYERS 1
#endif

Backbuffer::~Backbuffer() {
  vkDestroyFramebuffer(_device, framebuffer, nullptr);
  vkDestroyImageView(_device, view, nullptr);
}

static bool
validateExtensions(const vector<const char *> &required,
                   const std::vector<VkExtensionProperties> &available) {
  for (auto extension : required) {
    bool found = false;
    for (auto &availableExtension : available) {
      if (strcmp(availableExtension.extensionName, extension) == 0) {
        found = true;
        break;
      }
    }

    if (!found)
      return false;
  }

  return true;
}

struct PhysicalDevice {
  VkPhysicalDevice Gpu = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties Properties;
  VkPhysicalDeviceMemoryProperties MemoryProperties;
  std::vector<VkQueueFamilyProperties> QueueProperties;
  uint32_t SelectedQueueFamilyIndex = -1;

  bool SelectQueueFamily(VkPhysicalDevice gpu, VkSurfaceKHR surface) {
    Gpu = gpu;

    vkGetPhysicalDeviceProperties(Gpu, &Properties);
    vkGetPhysicalDeviceMemoryProperties(Gpu, &MemoryProperties);

    uint32_t queueCount;
    vkGetPhysicalDeviceQueueFamilyProperties(Gpu, &queueCount, nullptr);
    if (queueCount < 1) {
      LOGE("Failed to query number of queues.");
      return false;
    }

    QueueProperties.resize(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(Gpu, &queueCount,
                                             QueueProperties.data());

    bool foundQueue = false;
    for (uint32_t i = 0; i < queueCount; i++) {
      VkBool32 supportsPresent;
      vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &supportsPresent);

      // We want a queue which supports all of graphics, compute and
      // presentation.

      // There must exist at least one queue that has graphics and compute
      // support.
      const VkQueueFlags required =
          VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

      if (((QueueProperties[i].queueFlags & required) == required) &&
          supportsPresent) {
        SelectedQueueFamilyIndex = i;
        foundQueue = true;
        break;
      }
    }

    if (!foundQueue) {
      LOGE("Did not find suitable queue which supports graphics, compute and "
           "presentation.\n");
      return false;
    }

    return true;
  }
};

struct DeviceManager {
  VkInstance Instance;
  VkSurfaceKHR Surface = VK_NULL_HANDLE;
  std::vector<VkPhysicalDevice> Gpus;
  std::vector<VkPhysicalDeviceProperties> GpuProps;
  PhysicalDevice Selected = {};
  // created logical device
  VkDevice Device = VK_NULL_HANDLE;
  VkQueue Queue = VK_NULL_HANDLE;

  DeviceManager(VkInstance instance) : Instance(instance) {}

  ~DeviceManager() {
    if (Device) {
      vkDestroyDevice(Device, nullptr);
    }
    if (Surface) {
      vkDestroySurfaceKHR(Instance, Surface, nullptr);
    }
    if (Instance) {
      vkDestroyInstance(Instance, nullptr);
    }
  }

  /// create instance and validation layer
  static std::shared_ptr<DeviceManager> create() {
    const vector<const char *> requiredInstanceExtensions{
        "VK_KHR_surface", "VK_KHR_android_surface"};

    uint32_t instanceExtensionCount;
    vector<const char *> activeInstanceExtensions;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(
        nullptr, &instanceExtensionCount, nullptr));
    vector<VkExtensionProperties> instanceExtensions(instanceExtensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(
        nullptr, &instanceExtensionCount, instanceExtensions.data()));
    // for (auto &instanceExt : instanceExtensions)
    //   LOGI("Instance extension: %s\n", instanceExt.extensionName);

    bool useInstanceExtensions = true;
    if (!validateExtensions(requiredInstanceExtensions, instanceExtensions)) {
      LOGI("Required instance extensions are missing, will try without.\n");
      useInstanceExtensions = false;
    } else
      activeInstanceExtensions = requiredInstanceExtensions;

    bool haveDebugReport = false;
    for (auto &ext : instanceExtensions) {
      if (strcmp(ext.extensionName, "VK_EXT_debug_utils") == 0) {
        haveDebugReport = true;
        useInstanceExtensions = true;
        activeInstanceExtensions.push_back("VK_EXT_debug_utils");
        break;
      }
    }

    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Mali SDK";
    app.applicationVersion = 0;
    app.pEngineName = "Mali SDK";
    app.engineVersion = 0;
    app.apiVersion = VK_MAKE_VERSION(1, 0, 24);

    VkInstanceCreateInfo instanceInfo = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceInfo.pApplicationInfo = &app;
    if (useInstanceExtensions) {
      instanceInfo.enabledExtensionCount = activeInstanceExtensions.size();
      instanceInfo.ppEnabledExtensionNames = activeInstanceExtensions.data();
    }

    // Create the Vulkan instance

    // for (auto name : activeLayers) {
    //   LOGI("[layer] %s", name);
    // }
    // for (auto name : activeInstanceExtensions) {
    //   LOGI("[instance extension] %s", name);
    // }
    VkInstance instance;
    VkResult res = vkCreateInstance(&instanceInfo, nullptr, &instance);

    // Try to fall back to compatible Vulkan versions if the driver is using
    // older, but compatible API versions.
    if (res == VK_ERROR_INCOMPATIBLE_DRIVER) {
      app.apiVersion = VK_MAKE_VERSION(1, 0, 1);
      res = vkCreateInstance(&instanceInfo, nullptr, &instance);
      if (res == VK_SUCCESS) {
        LOGI("Created Vulkan instance with API version 1.0.1.\n");
      }
    }
    if (res == VK_ERROR_INCOMPATIBLE_DRIVER) {
      app.apiVersion = VK_MAKE_VERSION(1, 0, 2);
      res = vkCreateInstance(&instanceInfo, nullptr, &instance);
      if (res == VK_SUCCESS)
        LOGI("Created Vulkan instance with API version 1.0.2.\n");
    }
    if (res != VK_SUCCESS) {
      LOGE("Failed to create Vulkan instance (error: %d).\n", int(res));
      return {};
    }

    auto ptr = std::shared_ptr<DeviceManager>(new DeviceManager(instance));
    return ptr;
  }

  bool createSurfaceFromAndroid(ANativeWindow *window) {
    VkAndroidSurfaceCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .pNext = 0,
        .flags = 0,
        .window = window,
    };
    if (vkCreateAndroidSurfaceKHR(Instance, &info, nullptr, &Surface) !=
        VK_SUCCESS) {
      LOGE("vkCreateAndroidSurfaceKHR");
      return false;
    }

    return true;
  }

  VkPhysicalDevice selectGpu() {
    uint32_t gpuCount = 0;
    if (vkEnumeratePhysicalDevices(Instance, &gpuCount, nullptr) !=
        VK_SUCCESS) {
      LOGE("vkEnumeratePhysicalDevices");
      return VK_NULL_HANDLE;
    }
    if (gpuCount < 1) {
      LOGE("vkEnumeratePhysicalDevices: no device");
      return VK_NULL_HANDLE;
    }
    Gpus.resize(gpuCount);
    if (vkEnumeratePhysicalDevices(Instance, &gpuCount, Gpus.data()) !=
        VK_SUCCESS) {
      LOGE("vkEnumeratePhysicalDevices");
      return VK_NULL_HANDLE;
    }
    GpuProps.resize(gpuCount);
    for (uint32_t i = 0; i < gpuCount; ++i) {
      vkGetPhysicalDeviceProperties(Gpus[i], &GpuProps[i]);
      LOGI("[gpu: %02d] %s", i, GpuProps[i].deviceName);
    }
    if (gpuCount > 1) {
      LOGI("select 1st device");
    }
    if (!Selected.SelectQueueFamily(Gpus[0], Surface)) {
      return VK_NULL_HANDLE;
    }

    return Selected.Gpu;
  }

  bool createLogicalDevice() {
    uint32_t deviceExtensionCount;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(
        Selected.Gpu, nullptr, &deviceExtensionCount, nullptr));
    vector<VkExtensionProperties> deviceExtensions(deviceExtensionCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(
        Selected.Gpu, nullptr, &deviceExtensionCount, deviceExtensions.data()));
    // for (auto &deviceExt : deviceExtensions)
    //   LOGI("Device extension: %s\n", deviceExt.extensionName);
    const vector<const char *> requiredDeviceExtensions{"VK_KHR_swapchain"};
    bool useDeviceExtensions = true;
    if (!validateExtensions(requiredDeviceExtensions, deviceExtensions)) {
      LOGI("Required device extensions are missing, will try without.\n");
      useDeviceExtensions = false;
    }

    static const float one = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .queueFamilyIndex = Selected.SelectedQueueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &one,
    };

    VkPhysicalDeviceFeatures features = {false};
    VkDeviceCreateInfo deviceInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueInfo,
        .enabledExtensionCount =
            static_cast<uint32_t>(requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
        .pEnabledFeatures = &features,
    };

    if (vkCreateDevice(Selected.Gpu, &deviceInfo, nullptr, &Device) !=
        VK_SUCCESS) {
      return false;
    }

    vkGetDeviceQueue(Device, Selected.SelectedQueueFamilyIndex, 0, &Queue);

    return true;
  }
};

MaliSDK::Result Platform::initVulkan(ANativeWindow *window) {

  _device = DeviceManager::create();
  if (!_device) {
    return MaliSDK::RESULT_ERROR_GENERIC;
  }
  if (!_device->createSurfaceFromAndroid(window)) {
    return MaliSDK::RESULT_ERROR_GENERIC;
  }
  auto gpu = _device->selectGpu();
  if (!gpu) {
    return MaliSDK::RESULT_ERROR_GENERIC;
  }
  if (!_device->createLogicalDevice()) {
    return MaliSDK::RESULT_ERROR_GENERIC;
  }

  auto _res = initSwapchain();
  if (_res != MaliSDK::RESULT_SUCCESS) {
    LOGE("Failed to init swapchain.");
    return _res;
  }

  _res = onPlatformUpdate();
  if (_res != MaliSDK::RESULT_SUCCESS) {
    return _res;
  }

  semaphoreManager = new MaliSDK::SemaphoreManager(_device->Device);
  return MaliSDK::RESULT_SUCCESS;
}

// typedef VkBool32 (VKAPI_PTR *PFN_vkDebugUtilsMessengerCallbackEXT)(
//     VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
//     VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
//     const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
//     void*                                            pUserData);
static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageTypes,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {
  if (pCallbackData->flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
    LOGE("Validation Layer: Error: %s: %s\n", pCallbackData->pMessageIdName,
         pCallbackData->pMessage);
  } else if (pCallbackData->flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
    LOGE("Validation Layer: Warning: %s: %s\n", pCallbackData->pMessageIdName,
         pCallbackData->pMessage);
  } else if (pCallbackData->flags &
             VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
    LOGI("Validation Layer: Performance warning: %s: %s\n",
         pCallbackData->pMessageIdName, pCallbackData->pMessage);
  } else {
    LOGI("Validation Layer: Information: %s: %s\n",
         pCallbackData->pMessageIdName, pCallbackData->pMessage);
  }
  return VK_FALSE;
}

static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
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

std::shared_ptr<Platform> Platform::create(ANativeWindow *window) {
  auto ptr = std::shared_ptr<Platform>(new Platform);
  if (ptr->initVulkan(window) != MaliSDK::RESULT_SUCCESS) {
    LOGE("Failed to create Vulkan window.\n");
    abort();
  }
  return ptr;
}

VkDevice Platform::getDevice() const { return _device->Device; }

#define NELEMS(x) (sizeof(x) / sizeof((x)[0]))
static const char *pValidationLayers[] = {
    "VK_LAYER_KHRONOS_validation",
};

static void addSupportedLayers(vector<const char *> &activeLayers,
                               const vector<VkLayerProperties> &instanceLayers,
                               const char **ppRequested,
                               unsigned numRequested) {
  for (unsigned i = 0; i < numRequested; i++) {
    auto *layer = ppRequested[i];
    for (auto &ext : instanceLayers) {
      if (strcmp(ext.layerName, layer) == 0) {
        activeLayers.push_back(layer);
        break;
      }
    }
  }
}

Platform::~Platform() {
  // Don't release anything until the GPU is completely idle.
  vkDeviceWaitIdle(_device->Device);

  // Tear down backbuffers.
  // If our swapchain changes, we will call this, and create a new swapchain.
  // vkQueueWaitIdle(getGraphicsQueue());

  delete semaphoreManager;
  semaphoreManager = nullptr;

  destroySwapchain();

  // if (debug_callback) {
  //   vkDestroyDebugReportCallbackEXT(instance, debug_callback, nullptr);
  //   debug_callback = VK_NULL_HANDLE;
  // }
}

void Platform::destroySwapchain() {
  if (swapchain) {
    vkDestroySwapchainKHR(_device->Device, swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;
  }
}

const VkPhysicalDeviceMemoryProperties &Platform::getMemoryProperties() const {
  return _device->Selected.MemoryProperties;
}

MaliSDK::Result Platform::initSwapchain() {
  VkPhysicalDevice gpu = _device->Selected.Gpu;

  VkSurfaceCapabilitiesKHR surfaceProperties;
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, _device->Surface,
                                                     &surfaceProperties));

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, _device->Surface, &formatCount,
                                       nullptr);
  vector<VkSurfaceFormatKHR> formats(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, _device->Surface, &formatCount,
                                       formats.data());

  VkSurfaceFormatKHR format;
  format.format = VK_FORMAT_UNDEFINED;
  if (formatCount == 0) {
    LOGE("Surface has no formats.\n");
    return MaliSDK::RESULT_ERROR_GENERIC;
  } else if (formatCount == 1) {
    format = formats[0];
  } else {
    for (auto &candidate : formats) {
      switch (candidate.format) {
      // Favor UNORM formats as the samples are not written for sRGB currently.
      case VK_FORMAT_R8G8B8A8_UNORM:
      case VK_FORMAT_B8G8R8A8_UNORM:
      case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        format = candidate;
        break;

      default:
        break;
      }

      if (format.format != VK_FORMAT_UNDEFINED)
        break;
    }
  }

  if (format.format == VK_FORMAT_UNDEFINED) {
    LOGE("VK_FORMAT_UNDEFINED");
    abort();
  }

  VkExtent2D swapchainSize;
  // -1u is a magic value (in Vulkan specification) which means there's no fixed
  // size.
  if (surfaceProperties.currentExtent.width == -1u) {
    LOGE("-1 size");
    abort();
    // swapchainSize.width = dim.width;
    // swapchainSize.height = dim.height;
  } else {
    swapchainSize = surfaceProperties.currentExtent;
  }

  // FIFO must be supported by all implementations.
  VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

  // Determine the number of VkImage's to use in the swapchain.
  // Ideally, we desire to own 1 image at a time, the rest of the images can
  // either be rendered to and/or
  // being queued up for display.
  uint32_t desiredSwapchainImages = surfaceProperties.minImageCount + 1;
  if ((surfaceProperties.maxImageCount > 0) &&
      (desiredSwapchainImages > surfaceProperties.maxImageCount)) {
    // Application must settle for fewer images than desired.
    desiredSwapchainImages = surfaceProperties.maxImageCount;
  }

  // Figure out a suitable surface transform.
  VkSurfaceTransformFlagBitsKHR preTransform;
  if (surfaceProperties.supportedTransforms &
      VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  else
    preTransform = surfaceProperties.currentTransform;

  VkSwapchainKHR oldSwapchain = swapchain;

  // Find a supported composite type.
  VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  if (surfaceProperties.supportedCompositeAlpha &
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
    composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  else if (surfaceProperties.supportedCompositeAlpha &
           VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
    composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
  else if (surfaceProperties.supportedCompositeAlpha &
           VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
    composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
  else if (surfaceProperties.supportedCompositeAlpha &
           VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
    composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;

  VkSwapchainCreateInfoKHR info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  info.surface = _device->Surface;
  info.minImageCount = desiredSwapchainImages;
  info.imageFormat = format.format;
  info.imageColorSpace = format.colorSpace;
  info.imageExtent.width = swapchainSize.width;
  info.imageExtent.height = swapchainSize.height;
  info.imageArrayLayers = 1;
  info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.preTransform = preTransform;
  info.compositeAlpha = composite;
  info.presentMode = swapchainPresentMode;
  info.clipped = true;
  info.oldSwapchain = oldSwapchain;

  auto device = _device->Device;
  VK_CHECK(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain));

  if (oldSwapchain != VK_NULL_HANDLE)
    vkDestroySwapchainKHR(device, oldSwapchain, nullptr);

  swapchainDimensions.width = swapchainSize.width;
  swapchainDimensions.height = swapchainSize.height;
  swapchainDimensions.format = format.format;

  uint32_t imageCount;
  VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
  swapchainImages.resize(imageCount);
  VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount,
                                   swapchainImages.data()));

  return MaliSDK::RESULT_SUCCESS;
}

MaliSDK::Result Platform::acquireNextImage(unsigned *image) {
  if (swapchain == VK_NULL_HANDLE) {
    // Recreate swapchain.
    if (initSwapchain() == MaliSDK::RESULT_SUCCESS)
      return MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN;
    else
      return MaliSDK::RESULT_ERROR_GENERIC;
  }

  auto device = _device->Device;
  auto queue = _device->Queue;
  auto acquireSemaphore = semaphoreManager->getClearedSemaphore();
  VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                       acquireSemaphore, VK_NULL_HANDLE, image);

  if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
    vkQueueWaitIdle(queue);
    semaphoreManager->addClearedSemaphore(acquireSemaphore);

    // Recreate swapchain.
    if (initSwapchain() == MaliSDK::RESULT_SUCCESS)
      return MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN;
    else
      return MaliSDK::RESULT_ERROR_GENERIC;
  } else if (res != VK_SUCCESS) {
    vkQueueWaitIdle(queue);
    semaphoreManager->addClearedSemaphore(acquireSemaphore);
    return MaliSDK::RESULT_ERROR_GENERIC;
  } else {
    // Signal the underlying context that we're using this backbuffer now.
    // This will also wait for all fences associated with this swapchain image
    // to complete first.
    // When submitting command buffer that writes to swapchain, we need to wait
    // for this semaphore first.
    // Also, delete the older semaphore.
    auto oldSemaphore = beginFrame(*image, acquireSemaphore);

    // Recycle the old semaphore back into the semaphore manager.
    if (oldSemaphore != VK_NULL_HANDLE)
      semaphoreManager->addClearedSemaphore(oldSemaphore);

    return MaliSDK::RESULT_SUCCESS;
  }
}

MaliSDK::Result Platform::presentImage(unsigned index) {
  VkResult result;
  VkPresentInfoKHR present = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present.swapchainCount = 1;
  present.pSwapchains = &swapchain;
  present.pImageIndices = &index;
  present.pResults = &result;
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &getSwapchainReleaseSemaphore();

  VkResult res = vkQueuePresentKHR(_device->Queue, &present);

  if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
    return MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN;
  else if (res != VK_SUCCESS)
    return MaliSDK::RESULT_ERROR_GENERIC;
  else
    return MaliSDK::RESULT_SUCCESS;
}

void Platform::submitSwapchain(VkCommandBuffer cmd) {
  // For the first frames, we will create a release semaphore.
  // This can be reused every frame. Semaphores are reset when they have been
  // successfully been waited on.
  // If we aren't using acquire semaphores, we aren't using release semaphores
  // either.
  if (getSwapchainReleaseSemaphore() == VK_NULL_HANDLE &&
      getSwapchainAcquireSemaphore() != VK_NULL_HANDLE) {
    VkSemaphore releaseSemaphore;
    VkSemaphoreCreateInfo semaphoreInfo = {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VK_CHECK(vkCreateSemaphore(_device->Device, &semaphoreInfo, nullptr,
                               &releaseSemaphore));
    perFrame[swapchainIndex]->setSwapchainReleaseSemaphore(releaseSemaphore);
  }

  submitCommandBuffer(cmd, getSwapchainAcquireSemaphore(),
                      getSwapchainReleaseSemaphore());
}

void Platform::submitCommandBuffer(VkCommandBuffer cmd,
                                   VkSemaphore acquireSemaphore,
                                   VkSemaphore releaseSemaphore) {
  // All queue submissions get a fence that CPU will wait
  // on for synchronization purposes.
  VkFence fence = getFenceManager().requestClearedFence();

  VkSubmitInfo info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  info.commandBufferCount = 1;
  info.pCommandBuffers = &cmd;

  const VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  info.waitSemaphoreCount = acquireSemaphore != VK_NULL_HANDLE ? 1 : 0;
  info.pWaitSemaphores = &acquireSemaphore;
  info.pWaitDstStageMask = &waitStage;
  info.signalSemaphoreCount = releaseSemaphore != VK_NULL_HANDLE ? 1 : 0;
  info.pSignalSemaphores = &releaseSemaphore;

  VK_CHECK(vkQueueSubmit(_device->Queue, 1, &info, fence));
}

MaliSDK::Result Platform::onPlatformUpdate() {
  // waitIdle();

  // Initialize per-frame resources.
  // Every swapchain image has its own command pool and fence manager.
  // This makes it very easy to keep track of when we can reset command buffers
  // and such.
  perFrame.clear();
  for (unsigned i = 0; i < this->getNumSwapchainImages(); i++)
    perFrame.emplace_back(new MaliSDK::PerFrame(
        _device->Device, _device->Selected.SelectedQueueFamilyIndex));

  setRenderingThreadCount(renderingThreadCount);

  return MaliSDK::RESULT_SUCCESS;
}

VkCommandBuffer
Platform::beginRender(const std::shared_ptr<Backbuffer> &backbuffer) {
  // Request a fresh command buffer.
  VkCommandBuffer cmd = requestPrimaryCommandBuffer();

  // We will only submit this once before it's recycled.
  VkCommandBufferBeginInfo beginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  vkBeginCommandBuffer(cmd, &beginInfo);

  return cmd;
}

void Platform::updateSwapchain(VkRenderPass renderPass) {
  // Tear down backbuffers.
  // If our swapchain changes, we will call this, and create a new swapchain.
  vkQueueWaitIdle(_device->Queue);

  // In case we're reinitializing the swapchain, terminate the old one first.
  backbuffers.clear();

  auto device = _device->Device;

  // For all backbuffers in the swapchain ...
  for (uint32_t i = 0; i < swapchainImages.size(); ++i) {
    auto image = swapchainImages[i];
    auto backbuffer = std::make_shared<Backbuffer>(device, i);
    backbuffer->image = image;

    // Create an image view which we can render into.
    VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = swapchainDimensions.format;
    view.image = image;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.layerCount = 1;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.components.r = VK_COMPONENT_SWIZZLE_R;
    view.components.g = VK_COMPONENT_SWIZZLE_G;
    view.components.b = VK_COMPONENT_SWIZZLE_B;
    view.components.a = VK_COMPONENT_SWIZZLE_A;

    VK_CHECK(vkCreateImageView(device, &view, nullptr, &backbuffer->view));

    // Build the framebuffer.
    VkFramebufferCreateInfo fbInfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbInfo.renderPass = renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &backbuffer->view;
    fbInfo.width = swapchainDimensions.width;
    fbInfo.height = swapchainDimensions.height;
    fbInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(device, &fbInfo, nullptr,
                                 &backbuffer->framebuffer));

    backbuffers.push_back(backbuffer);
  }
}

void Platform::setRenderingThreadCount(unsigned count) {
  vkQueueWaitIdle(_device->Queue);
  for (auto &pFrame : perFrame)
    pFrame->setSecondaryCommandManagersCount(count);
  renderingThreadCount = count;
}

#pragma once
#include <vulkan/vulkan.h>

#ifdef XR_USE_PLATFORM_WIN32
#include <Unknwn.h>
#endif
#ifdef XR_USE_PLATFORM_ANDROID
#include <android_native_app_glue.h>
#endif

#include <openxr/openxr_platform.h>

#include <array>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct Cube {
  XrPosef Pose;
  XrVector3f Scale;
};

// Wraps a graphics API so the main openxr program can be graphics
// API-independent.
struct VulkanGraphicsPlugin {
  VulkanGraphicsPlugin(const struct Options &options,
                       std::shared_ptr<struct IPlatformPlugin> /*unused*/);

  // OpenXR extensions required by this graphics API.
  std::vector<std::string> GetInstanceExtensions() const {
    return {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
  }

  // Create an instance of this graphics api for the provided instance and
  // systemId.
  void InitializeDevice(XrInstance instance, XrSystemId systemId);

  // Select the preferred swapchain format from the list of available formats.
  int64_t
  SelectColorSwapchainFormat(const std::vector<int64_t> &runtimeFormats) const;

  // Get the graphics binding header for session creation.
  const XrBaseInStructure *GetGraphicsBinding() const {
    return reinterpret_cast<const XrBaseInStructure *>(&m_graphicsBinding);
  }

  // Allocate space for the swapchain image structures. These are different for
  // each graphics API. The returned pointers are valid for the lifetime of the
  // graphics plugin.
  std::vector<XrSwapchainImageBaseHeader *> AllocateSwapchainImageStructs(
      uint32_t capacity, const XrSwapchainCreateInfo &swapchainCreateInfo);

  // Render to a swapchain image for a projection view.
  void RenderView(const XrCompositionLayerProjectionView &layerView,
                  const XrSwapchainImageBaseHeader *swapchainImage,
                  int64_t /*swapchainFormat*/, const std::vector<Cube> &cubes);

  // Get recommended number of sub-data element samples in view
  // (recommendedSwapchainSampleCount) if supported by the graphics plugin. A
  // supported value otherwise.
  // uint32_t
  // GetSupportedSwapchainSampleCount(const XrViewConfigurationView &view) {
  //   return view.recommendedSwapchainSampleCount;
  // }
  uint32_t GetSupportedSwapchainSampleCount(const XrViewConfigurationView &) {
    return VK_SAMPLE_COUNT_1_BIT;
  }

  // Perform required steps after updating Options
  void UpdateOptions(const struct Options &options);

  // Note: The output must not outlive the input - this modifies the input and
  // returns a collection of views into that modified input!
  std::vector<const char *> ParseExtensionString(char *names);

  const char *GetValidationLayerName();

#ifdef USE_ONLINE_VULKAN_SHADERC
  // Compile a shader to a SPIR-V binary.
  std::vector<uint32_t> CompileGlslShader(const std::string &name,
                                          shaderc_shader_kind kind,
                                          const std::string &source);
#endif

  void InitializeResources();

  XrGraphicsBindingVulkan2KHR m_graphicsBinding{
      XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
  std::list<std::shared_ptr<class SwapchainImageContext>>
      m_swapchainImageContexts;
  std::map<const XrSwapchainImageBaseHeader *,
           std::shared_ptr<SwapchainImageContext>>
      m_swapchainImageContextMap;

  VkInstance m_vkInstance{VK_NULL_HANDLE};
  VkPhysicalDevice m_vkPhysicalDevice{VK_NULL_HANDLE};
  VkDevice m_vkDevice{VK_NULL_HANDLE};
  uint32_t m_queueFamilyIndex = 0;
  VkQueue m_vkQueue{VK_NULL_HANDLE};
  VkSemaphore m_vkDrawDone{VK_NULL_HANDLE};

  std::shared_ptr<class MemoryAllocator> m_memAllocator;
  std::shared_ptr<class ShaderProgram> m_shaderProgram{};
  std::shared_ptr<class CmdBuffer> m_cmdBuffer;
  std::shared_ptr<class PipelineLayout> m_pipelineLayout{};
  std::shared_ptr<struct VertexBuffer> m_drawBuffer;
  std::array<float, 4> m_clearColor;

#if defined(USE_MIRROR_WINDOW)
  Swapchain m_swapchain{};
#endif

  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT{nullptr};
  VkDebugUtilsMessengerEXT m_vkDebugUtilsMessenger{VK_NULL_HANDLE};

  VkBool32
  debugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT messageTypes,
               const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData);

  XrStructureType GetGraphicsBindingType() const {
    return XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR;
  }

  XrStructureType GetSwapchainImageType() const {
    return XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR;
  }

  XrResult
  CreateVulkanInstanceKHR(XrInstance instance,
                          const XrVulkanInstanceCreateInfoKHR *createInfo,
                          VkInstance *vulkanInstance, VkResult *vulkanResult);

  XrResult CreateVulkanDeviceKHR(XrInstance instance,
                                 const XrVulkanDeviceCreateInfoKHR *createInfo,
                                 VkDevice *vulkanDevice,
                                 VkResult *vulkanResult);

  XrResult
  GetVulkanGraphicsDevice2KHR(XrInstance instance,
                              const XrVulkanGraphicsDeviceGetInfoKHR *getInfo,
                              VkPhysicalDevice *vulkanPhysicalDevice);

  XrResult GetVulkanGraphicsRequirements2KHR(
      XrInstance instance, XrSystemId systemId,
      XrGraphicsRequirementsVulkan2KHR *graphicsRequirements);
};

#pragma once
#include "graphicsplugin_vulkan.h"

#include <vulkan/vulkan.h>

#include <Unknwn.h>

#include <openxr/openxr_platform.h>

#include <array>
#include <list>
#include <map>

struct VulkanGraphicsPlugin : public IGraphicsPlugin {
  VulkanGraphicsPlugin(const std::shared_ptr<Options> &options,
                       std::shared_ptr<struct IPlatformPlugin> /*unused*/);

  std::vector<std::string> GetInstanceExtensions() const override {
    return {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
  }

  // Note: The output must not outlive the input - this modifies the input and
  // returns a collection of views into that modified input!
  std::vector<const char *> ParseExtensionString(char *names);

  const char *GetValidationLayerName();

  void InitializeDevice(XrInstance instance, XrSystemId systemId) override;

#ifdef USE_ONLINE_VULKAN_SHADERC
  // Compile a shader to a SPIR-V binary.
  std::vector<uint32_t> CompileGlslShader(const std::string &name,
                                          shaderc_shader_kind kind,
                                          const std::string &source);
#endif

  void InitializeResources();

  int64_t SelectColorSwapchainFormat(
      const std::vector<int64_t> &runtimeFormats) const override;

  const XrBaseInStructure *GetGraphicsBinding() const override {
    return reinterpret_cast<const XrBaseInStructure *>(&m_graphicsBinding);
  }

  std::vector<XrSwapchainImageBaseHeader *> AllocateSwapchainImageStructs(
      uint32_t capacity,
      const XrSwapchainCreateInfo &swapchainCreateInfo) override;

  void RenderView(const XrCompositionLayerProjectionView &layerView,
                  const XrSwapchainImageBaseHeader *swapchainImage,
                  int64_t /*swapchainFormat*/,
                  const std::vector<Cube> &cubes) override;

  uint32_t
  GetSupportedSwapchainSampleCount(const XrViewConfigurationView &) override {
    return VK_SAMPLE_COUNT_1_BIT;
  }

  void UpdateOptions(const std::shared_ptr<Options> &options) override;

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

  virtual XrStructureType GetGraphicsBindingType() const {
    return XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR;
  }

  virtual XrStructureType GetSwapchainImageType() const {
    return XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR;
  }

  virtual XrResult
  CreateVulkanInstanceKHR(XrInstance instance,
                          const XrVulkanInstanceCreateInfoKHR *createInfo,
                          VkInstance *vulkanInstance, VkResult *vulkanResult);

  virtual XrResult
  CreateVulkanDeviceKHR(XrInstance instance,
                        const XrVulkanDeviceCreateInfoKHR *createInfo,
                        VkDevice *vulkanDevice, VkResult *vulkanResult);

  virtual XrResult
  GetVulkanGraphicsDevice2KHR(XrInstance instance,
                              const XrVulkanGraphicsDeviceGetInfoKHR *getInfo,
                              VkPhysicalDevice *vulkanPhysicalDevice);

  virtual XrResult GetVulkanGraphicsRequirements2KHR(
      XrInstance instance, XrSystemId systemId,
      XrGraphicsRequirementsVulkan2KHR *graphicsRequirements);
};

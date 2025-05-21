#pragma once
#include "VertexBuffer.h"
#include <openxr/openxr.h>
#include <vulkan/vulkan.h>

#include <array>
#include <list>
#include <map>
#include <memory>
#include <vector>

// Wraps a graphics API so the main openxr program can be graphics
// API-independent.
struct VulkanGraphicsPlugin {
  VulkanGraphicsPlugin(const struct Options &options,
                       std::shared_ptr<struct IPlatformPlugin> /*unused*/);

  // Create an instance of this graphics api for the provided instance and
  // systemId.
  void InitializeDevice(VkInstance instance, VkPhysicalDevice physicalDevice,
                        VkDevice device, VkDeviceQueueCreateInfo queueInfo,
                        VkDebugUtilsMessengerCreateInfoEXT debugInfo);

  // Select the preferred swapchain format from the list of available formats.
  int64_t
  SelectColorSwapchainFormat(const std::vector<int64_t> &runtimeFormats) const;

  // Render to a swapchain image for a projection view.
  void RenderView(
      const XrCompositionLayerProjectionView &layerView,
      const std::shared_ptr<class SwapchainImageContext> &swapchainContext,
      uint32_t imageIndex, int64_t /*swapchainFormat*/,
      const std::vector<Cube> &cubes);

  // Perform required steps after updating Options
  void UpdateOptions(const struct Options &options);

  // Note: The output must not outlive the input - this modifies the input and
  // returns a collection of views into that modified input!
  std::vector<const char *> ParseExtensionString(char *names);

#ifdef USE_ONLINE_VULKAN_SHADERC
  // Compile a shader to a SPIR-V binary.
  std::vector<uint32_t> CompileGlslShader(const std::string &name,
                                          shaderc_shader_kind kind,
                                          const std::string &source);
#endif

  void InitializeResources();
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
};

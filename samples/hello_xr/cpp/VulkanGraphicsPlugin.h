#pragma once
#include "VertexBuffer.h"
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <list>
#include <map>

// Wraps a graphics API so the main openxr program can be graphics
// API-independent.
class VulkanGraphicsPlugin {
public:
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
  std::shared_ptr<class Swapchain> m_swapchain;

private:
  VkDebugUtilsMessengerEXT m_vkDebugUtilsMessenger{VK_NULL_HANDLE};

public:
  VulkanGraphicsPlugin(VkInstance instance, VkPhysicalDevice physicalDevice,
                       VkDevice device, uint32_t queueFamilyIndex,
                       VkDebugUtilsMessengerCreateInfoEXT debugInfo);

  // Select the preferred swapchain format from the list of available formats.
  int64_t
  SelectColorSwapchainFormat(const std::vector<int64_t> &runtimeFormats) const;

  // Render to a swapchain image for a projection view.
  VkCommandBuffer BeginCommand();
  void EndCommand(VkCommandBuffer cmd);

  void RenderView(
      VkCommandBuffer cmd,
      const std::shared_ptr<class SwapchainImageContext> &swapchainContext,
      uint32_t imageIndex, const Vec4 &clearColor,
      const std::vector<Mat4> &cubes);

  void InitializeResources();

  VkBool32
  debugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT messageTypes,
               const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData);
};

#pragma once
#include <memory>
#include <string>
#include <vector>
#include <vko.h>

class PipelineImpl;
class VulkanFramework {
public:
  vko::Instance Instance;
  vko::Instance::SelectedPhysicalDevice _picked;
  vko::Device Device;
  std::shared_ptr<vko::Surface> Surface;
  std::shared_ptr<vko::Swapchain> Swapchain;
  VkQueue _graphicsQueue = VK_NULL_HANDLE;
  std::shared_ptr<vko::Fence> SubmitCompleteFence;

  std::vector<std::shared_ptr<vko::SwapchainFramebuffer>> _images;

private:
  std::shared_ptr<PipelineImpl> Pipeline;

  std::string _appName;
  std::string _engineName;

private:
  VkQueue GraphicsQueue = VK_NULL_HANDLE;
  VkQueue PresentQueue = VK_NULL_HANDLE;
  void *AssetManager = nullptr;

public:
  VulkanFramework(const char *appName, const char *engineName);
  ~VulkanFramework();
  void setAssetManager(void *p) { AssetManager = p; }
  bool
  initializeInstance(const std::vector<const char *> &layerNames,
                     const std::vector<const char *> &instanceExtensionNames);

#ifdef ANDROID
  bool createSurfaceAndroid(void *window);
#else
#endif
  bool initializeDevice(const std::vector<const char *> &layerNames,
                        const std::vector<const char *> &deviceExtensionNames,
                        VkSurfaceKHR surface);

  bool drawFrame(uint32_t width, uint32_t height);
};

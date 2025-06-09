#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

class SwapchainCommand {
  VkPhysicalDevice _physicalDevice;
  VkDevice _device;
  uint32_t _graphicsQueueFamilyIndex;

  std::vector<std::shared_ptr<class PerImageObject>> _backbuffers;
  VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> _descriptorSets;
  VkCommandPool _commandPool;
  std::vector<VkCommandBuffer> _commandBuffers;

public:
  SwapchainCommand(VkPhysicalDevice physicalDevice, VkDevice device,
                   uint32_t graphicsQueueFamilyIndex);
  ~SwapchainCommand();
  void clearSwapchainDependent();
  void createSwapchainDependent(VkExtent2D swapchainExtent,
                                uint32_t swapchainImageCount,
                                VkDescriptorSetLayout descriptorSetLayout);
  std::tuple<VkCommandBuffer, VkDescriptorSet, std::shared_ptr<PerImageObject>>
  frameResource(uint32_t currentImage);
  std::shared_ptr<PerImageObject>
  createFrameResource(uint32_t currentImage, VkImage swapchainImage,
                      VkExtent2D swapchainExtent, VkFormat format,
                      VkRenderPass renderpas);
};

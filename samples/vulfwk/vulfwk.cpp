#include "vulfwk.h"
#include "vulfwk_pipeline.h"
#include <vko.h>

struct SwapchainImage {
  VkDevice _device;
  VkImageView _imageView;
  VkFramebuffer _framebuffer;

  SwapchainImage(VkDevice device, VkImageView imageView,
                 VkFramebuffer framebuffer)
      : _device(device), _imageView(imageView), _framebuffer(framebuffer) {}

  ~SwapchainImage() {
    vkDestroyFramebuffer(_device, _framebuffer, nullptr);
    vkDestroyImageView(_device, _imageView, nullptr);
  }

  static std::shared_ptr<SwapchainImage> create(VkDevice device, VkImage image,
                                                VkExtent2D extent,
                                                VkFormat format,
                                                VkRenderPass renderPass) {
    VkImageViewCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .a = VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1},
    };
    VkImageView imageView;
    VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &imageView));

    VkImageView attachments[] = {imageView};
    VkFramebufferCreateInfo framebufferInfo{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderPass,
        .attachmentCount = 1,
        .pAttachments = attachments,
        .width = extent.width,
        .height = extent.height,
        .layers = 1,
    };
    VkFramebuffer framebuffer;
    VK_CHECK(
        vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer));

    auto ptr = std::make_shared<SwapchainImage>(device, imageView, framebuffer);
    return ptr;
  }
};

VulkanFramework::VulkanFramework(const char *appName, const char *engineName) {
  _appName = appName;
  _engineName = engineName;
  LOGI("*** VulkanFramewor::VulkanFramework ***");
}

VulkanFramework::~VulkanFramework() {
  LOGI("*** VulkanFramewor::~VulkanFramework ***");
  vkDeviceWaitIdle(Device);
}

bool VulkanFramework::initializeInstance(
    const std::vector<const char *> &layerNames,
    const std::vector<const char *> &extensionNames) {
  Instance._validationLayers = layerNames;
  Instance._instanceExtensions = extensionNames;
  Instance._appInfo.pApplicationName = _appName.c_str();
  Instance._appInfo.pEngineName = _engineName.c_str();
  if (Instance.create() != VK_SUCCESS) {
    LOGE("failed to create instance!");
    return false;
  }
  return true;
}

#ifdef ANDROID
#include <android_native_app_glue.h>
bool VulkanFramework::createSurfaceAndroid(void *p) {
  auto pNativeWindow = reinterpret_cast<ANativeWindow *>(p);

  VkAndroidSurfaceCreateInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
      .flags = 0,
      .window = pNativeWindow,
  };
  if (vkCreateAndroidSurfaceKHR(Instance, &info, nullptr, &Surface) !=
      VK_SUCCESS) {
    LOGE("failed: vkCreateAndroidSurfaceKHR");
    return false;
  }
  return true;
}
#else
#endif

bool VulkanFramework::initializeDevice(
    const std::vector<const char *> &layerNames,
    const std::vector<const char *> &deviceExtensionNames,
    VkSurfaceKHR surface) {

  _picked = Instance.pickPhysicakDevice(surface);
  Surface =
      std::make_shared<vko::Surface>(Instance, surface, _picked.physicalDevice);

  Device._validationLayers = Instance._validationLayers;
  VK_CHECK(Device.create(_picked.physicalDevice, _picked.graphicsFamily,
                         _picked.presentFamily));

  Pipeline = PipelineImpl::create(_picked.physicalDevice, surface, Device,
                                  Surface->chooseSwapSurfaceFormat().format,
                                  AssetManager);
  if (!Pipeline) {
    return false;
  }

  SubmitCompleteFence = std::make_shared<vko::Fence>(Device, true);
  vkGetDeviceQueue(Device, _picked.graphicsFamily, 0, &_graphicsQueue);

  return true;
}

bool VulkanFramework::drawFrame(uint32_t width, uint32_t height) {

  if (!Swapchain) {
    vkDeviceWaitIdle(Device);

    Swapchain = std::make_shared<vko::Swapchain>(Device);
    Swapchain->create(_picked.physicalDevice, Surface->_surface,
                      Surface->chooseSwapSurfaceFormat(),
                      Surface->chooseSwapPresentMode(), _picked.graphicsFamily,
                      _picked.presentFamily);

    _images.clear();
    _images.resize(Swapchain->_images.size());
  }

  auto acquired = Swapchain->acquireNextImage();

  auto image = _images[acquired.imageIndex];
  if (!image) {
    image = SwapchainImage::create(
        Device, acquired.image, Swapchain->_createInfo.imageExtent,
        Swapchain->_createInfo.imageFormat, Pipeline->renderPass());
    _images[acquired.imageIndex] = image;
  }

  Pipeline->draw(acquired.commandBuffer, acquired.imageIndex,
                 image->_framebuffer, Swapchain->_createInfo.imageExtent);

  VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &acquired.imageAvailableSemaphore->_semaphore,
      .pWaitDstStageMask = &waitDstStageMask,
      .commandBufferCount = 1,
      .pCommandBuffers = &acquired.commandBuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &acquired.submitCompleteSemaphore->_semaphore,
  };

  SubmitCompleteFence->reset();
  VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, *SubmitCompleteFence));

  SubmitCompleteFence->block();
  VK_CHECK(Swapchain->present(acquired.imageIndex,
                              acquired.imageAvailableSemaphore));

  return true;
}

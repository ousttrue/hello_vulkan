#include "application.hpp"
#include "common.hpp"
#include "pipeline.hpp"
#include "platform.hpp"

VulkanApplication::VulkanApplication(VkDevice device,
                                     MaliSDK::Platform *platform)
    : _device(device), pContext(platform) {
  LOGI("[VulkanApplication::VulkanApplication]");
}

std::shared_ptr<VulkanApplication>
VulkanApplication::create(MaliSDK::Platform *platform, VkFormat format,
                          AAssetManager *assetManager) {
  auto ptr = std::shared_ptr<VulkanApplication>(
      new VulkanApplication(platform->getDevice(), platform));
  ptr->initPipeline(format, assetManager);
  ptr->updateSwapchain(platform->swapchainImages,
                       platform->swapchainDimensions);
  return ptr;
}

void VulkanApplication::initPipeline(VkFormat format,
                                     AAssetManager *assetManager) {
  _pipeline = Pipeline::create(_device, format, assetManager);
  _pipeline->initVertexBuffer(pContext->getMemoryProperties());
}

void VulkanApplication::render(const std::shared_ptr<Backbuffer> &backbuffer,
                               uint32_t width, uint32_t height) {
  // Request a fresh command buffer.
  VkCommandBuffer cmd = pContext->requestPrimaryCommandBuffer();

  // We will only submit this once before it's recycled.
  VkCommandBufferBeginInfo beginInfo = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &beginInfo);

  _pipeline->render(cmd, backbuffer->framebuffer, width, height);

  // Submit it to the queue.
  pContext->submitSwapchain(cmd);
}

void VulkanApplication::termBackbuffers() {
  // Tear down backbuffers.
  // If our swapchain changes, we will call this, and create a new swapchain.

  if (!backbuffers.empty()) {
    // Wait until device is idle before teardown.
    vkQueueWaitIdle(pContext->getGraphicsQueue());
    for (auto &backbuffer : backbuffers) {
      vkDestroyFramebuffer(_device, backbuffer->framebuffer, nullptr);
      vkDestroyImageView(_device, backbuffer->view, nullptr);
    }
    backbuffers.clear();
  }
}

void VulkanApplication::terminate() {
  vkDeviceWaitIdle(_device);

  termBackbuffers();
}

void VulkanApplication::updateSwapchain(
    const std::vector<VkImage> &newBackbuffers,
    const MaliSDK::SwapchainDimensions &dim) {
  width = dim.width;
  height = dim.height;

  // In case we're reinitializing the swapchain, terminate the old one first.
  termBackbuffers();

  // For all backbuffers in the swapchain ...
  for (uint32_t i = 0; i < newBackbuffers.size(); ++i) {
    auto image = newBackbuffers[i];
    auto backbuffer = std::make_shared<Backbuffer>(i);
    backbuffer->image = image;

    // Create an image view which we can render into.
    VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = dim.format;
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

    VK_CHECK(vkCreateImageView(_device, &view, nullptr, &backbuffer->view));

    // Build the framebuffer.
    VkFramebufferCreateInfo fbInfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbInfo.renderPass = _pipeline->renderPass();
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &backbuffer->view;
    fbInfo.width = width;
    fbInfo.height = height;
    fbInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(_device, &fbInfo, nullptr,
                                 &backbuffer->framebuffer));

    backbuffers.push_back(backbuffer);
  }
}

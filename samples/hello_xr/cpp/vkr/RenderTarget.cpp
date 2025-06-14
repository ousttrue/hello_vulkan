#include "RenderTarget.h"
#include <array>
#include <stdexcept>
#include <vko/vko.h>

//
// VkImage + framebuffer wrapper
//
RenderTarget::~RenderTarget() {
  if (m_vkDevice != nullptr) {
    if (fb != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(m_vkDevice, fb, nullptr);
    }
    if (colorView != VK_NULL_HANDLE) {
      vkDestroyImageView(m_vkDevice, colorView, nullptr);
    }
    if (depthView != VK_NULL_HANDLE) {
      vkDestroyImageView(m_vkDevice, depthView, nullptr);
    }
  }

  // Note we don't own color/depthImage, it will get destroyed when
  // xrDestroySwapchain is called
  colorImage = VK_NULL_HANDLE;
  depthImage = VK_NULL_HANDLE;
  colorView = VK_NULL_HANDLE;
  depthView = VK_NULL_HANDLE;
  fb = VK_NULL_HANDLE;
  m_vkDevice = nullptr;
}

RenderTarget::RenderTarget(RenderTarget &&other) noexcept : RenderTarget() {
  using std::swap;
  swap(colorImage, other.colorImage);
  swap(depthImage, other.depthImage);
  swap(colorView, other.colorView);
  swap(depthView, other.depthView);
  swap(fb, other.fb);
  swap(m_vkDevice, other.m_vkDevice);
}

RenderTarget &RenderTarget::operator=(RenderTarget &&other) noexcept {
  if (&other == this) {
    return *this;
  }
  // Clean up ourselves.
  this->~RenderTarget();
  using std::swap;
  swap(colorImage, other.colorImage);
  swap(depthImage, other.depthImage);
  swap(colorView, other.colorView);
  swap(depthView, other.depthView);
  swap(fb, other.fb);
  swap(m_vkDevice, other.m_vkDevice);
  return *this;
}

std::shared_ptr<RenderTarget>
RenderTarget::Create(VkDevice device, VkImage aColorImage, VkImage aDepthImage,
                     VkExtent2D size, VkFormat colorFormat,
                     VkFormat depthFormat, VkRenderPass renderPass) {

  auto ptr = std::shared_ptr<RenderTarget>(new RenderTarget);

  ptr->m_vkDevice = device;

  ptr->colorImage = aColorImage;
  ptr->depthImage = aDepthImage;

  std::array<VkImageView, 2> attachments{};
  uint32_t attachmentCount = 0;

  // Create color image view
  if (ptr->colorImage != VK_NULL_HANDLE) {
    VkImageViewCreateInfo colorViewInfo{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    colorViewInfo.image = ptr->colorImage;
    colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorViewInfo.format = colorFormat;
    colorViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    colorViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    colorViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    colorViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorViewInfo.subresourceRange.baseMipLevel = 0;
    colorViewInfo.subresourceRange.levelCount = 1;
    colorViewInfo.subresourceRange.baseArrayLayer = 0;
    colorViewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(ptr->m_vkDevice, &colorViewInfo, nullptr,
                          &ptr->colorView) != VK_SUCCESS) {
      throw std::runtime_error("vkCreateImageView");
    }
    if (SetDebugUtilsObjectNameEXT(device, VK_OBJECT_TYPE_IMAGE_VIEW,
                                   (uint64_t)ptr->colorView,
                                   "hello_xr color image view") != VK_SUCCESS) {
      throw std::runtime_error("SetDebugUtilsObjectNameEXT");
    }
    attachments[attachmentCount++] = ptr->colorView;
  }

  // Create depth image view
  if (ptr->depthImage != VK_NULL_HANDLE) {
    VkImageViewCreateInfo depthViewInfo{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    depthViewInfo.image = ptr->depthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = depthFormat;
    depthViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    depthViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    depthViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    depthViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(ptr->m_vkDevice, &depthViewInfo, nullptr,
                          &ptr->depthView) != VK_SUCCESS) {
      throw std::runtime_error("vkCreateImageView");
    }
    if (SetDebugUtilsObjectNameEXT(device, VK_OBJECT_TYPE_IMAGE_VIEW,
                                   (uint64_t)ptr->depthView,
                                   "hello_xr depth image view") != VK_SUCCESS) {
      throw std::runtime_error("SetDebugUtilsObjectNameEXT");
    }
    attachments[attachmentCount++] = ptr->depthView;
  }

  VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  fbInfo.renderPass = renderPass;
  fbInfo.attachmentCount = attachmentCount;
  fbInfo.pAttachments = attachments.data();
  fbInfo.width = size.width;
  fbInfo.height = size.height;
  fbInfo.layers = 1;
  if (vkCreateFramebuffer(ptr->m_vkDevice, &fbInfo, nullptr, &ptr->fb) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateFramebuffer");
  }
  if (SetDebugUtilsObjectNameEXT(device, VK_OBJECT_TYPE_FRAMEBUFFER,
                                 (uint64_t)ptr->fb,
                                 "hello_xr framebuffer") != VK_SUCCESS) {
    throw std::runtime_error("SetDebugUtilsObjectNameEXT");
  }
  return ptr;
}

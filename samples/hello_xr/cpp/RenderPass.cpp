#include "RenderPass.h"
#include <array>
#include <common/vulkan_debug_object_namer.hpp>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

std::shared_ptr<RenderPass>
RenderPass::Create(const VulkanDebugObjectNamer &namer, VkDevice device,
                   VkFormat aColorFmt, VkFormat aDepthFmt) {

  auto ptr = std::shared_ptr<RenderPass>(new RenderPass);

  ptr->m_vkDevice = device;
  ptr->colorFmt = aColorFmt;
  ptr->depthFmt = aDepthFmt;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

  VkAttachmentReference colorRef = {0,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkAttachmentReference depthRef = {
      1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

  std::array<VkAttachmentDescription, 2> at = {};

  VkRenderPassCreateInfo rpInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .attachmentCount = 0,
      .pAttachments = at.data(),
      .subpassCount = 1,
      .pSubpasses = &subpass,
  };

  if (ptr->colorFmt != VK_FORMAT_UNDEFINED) {
    colorRef.attachment = rpInfo.attachmentCount++;

    at[colorRef.attachment].format = ptr->colorFmt;
    at[colorRef.attachment].samples = VK_SAMPLE_COUNT_1_BIT;
    at[colorRef.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    at[colorRef.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    at[colorRef.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    at[colorRef.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    at[colorRef.attachment].initialLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    at[colorRef.attachment].finalLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
  }

  if (ptr->depthFmt != VK_FORMAT_UNDEFINED) {
    depthRef.attachment = rpInfo.attachmentCount++;

    at[depthRef.attachment].format = ptr->depthFmt;
    at[depthRef.attachment].samples = VK_SAMPLE_COUNT_1_BIT;
    at[depthRef.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    at[depthRef.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    at[depthRef.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    at[depthRef.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    at[depthRef.attachment].initialLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    at[depthRef.attachment].finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    subpass.pDepthStencilAttachment = &depthRef;
  }

  if (vkCreateRenderPass(ptr->m_vkDevice, &rpInfo, nullptr, &ptr->pass) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateRenderPass");
  }
  if (namer.SetName(VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)ptr->pass,
                    "hello_xr render pass") != VK_SUCCESS) {
    throw std::runtime_error("->SetName");
  }

  return ptr;
}

RenderPass::~RenderPass() {
  if (m_vkDevice != nullptr) {
    if (pass != VK_NULL_HANDLE) {
      vkDestroyRenderPass(m_vkDevice, pass, nullptr);
    }
  }
  pass = VK_NULL_HANDLE;
  m_vkDevice = nullptr;
}

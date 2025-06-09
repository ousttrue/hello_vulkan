#pragma once
#include <memory>
#include <vulkan/vulkan.h>

struct RenderPass {
  VkDevice _device;
  VkRenderPass _renderPass;

  VkAttachmentDescription colorAttachment{
      // .format = swapchainFormat,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      // clear framebuffer to black before drawing next frame
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      // store rendered content to memory for later use
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };
  VkAttachmentReference colorAttachmentRef{
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };
  VkSubpassDescription subpass{
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentRef,
  };
  // dependency ensures that render passes do not start until an image is
  // available
  VkSubpassDependency dependency{
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };
  VkRenderPassCreateInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &colorAttachment,
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 1,
      .pDependencies = &dependency,
  };

  RenderPass(VkDevice device, VkFormat swapchainFormat) : _device(device) {
    colorAttachment.format = swapchainFormat;

    if (vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create render pass!");
    }
  }

  ~RenderPass() { vkDestroyRenderPass(_device, _renderPass, nullptr); }

  static std::shared_ptr<RenderPass> create(VkDevice device,
                                            VkFormat swapchainFormat) {
    auto ptr =
        std::shared_ptr<RenderPass>(new RenderPass(device, swapchainFormat));
    return ptr;
  }
};

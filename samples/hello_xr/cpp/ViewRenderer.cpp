#include "ViewRenderer.h"
#include "xr_linear.h"
#include <vuloxr/vk/command.h>

ViewRenderer::ViewRenderer(VkDevice _device, uint32_t queueFamilyIndex,

                           vuloxr::vk::Pipeline _pipeline)
    : device(_device), execFence(_device, true),
      pipeline(std::move(_pipeline)) {
  vkGetDeviceQueue(this->device, queueFamilyIndex, 0, &this->queue);

  VkCommandPoolCreateInfo cmdPoolInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queueFamilyIndex,
  };
  vuloxr::vk::CheckVkResult(vkCreateCommandPool(this->device, &cmdPoolInfo,
                                                nullptr, &this->commandPool));
  // if (vko::SetDebugUtilsObjectNameEXT(
  //         device, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)this->commandPool,
  //         "hello_xr command pool") != VK_SUCCESS) {
  //   throw std::runtime_error("SetDebugUtilsObjectNameEXT");
  // }

  VkCommandBufferAllocateInfo cmd{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = this->commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  vuloxr::vk::CheckVkResult(
      vkAllocateCommandBuffers(this->device, &cmd, &this->commandBuffer));
  // if (vko::SetDebugUtilsObjectNameEXT(device,
  // VK_OBJECT_TYPE_COMMAND_BUFFER,
  //                                     (uint64_t)this->commandBuffer,
  //                                     "hello_xr command buffer") !=
  //     VK_SUCCESS) {
  //   throw std::runtime_error("SetDebugUtilsObjectNameEXT");
  // }
}

ViewRenderer::~ViewRenderer() {
  vkFreeCommandBuffers(this->device, this->commandPool, 1,
                       &this->commandBuffer);
  vkDestroyCommandPool(this->device, this->commandPool, nullptr);
}

void ViewRenderer::render(const vuloxr::vk::PhysicalDevice &physicalDevice,
                          VkImage image, VkExtent2D extent,
                          VkFormat colorFormat, VkFormat depthFormat,
                          VkSampleCountFlagBits sampleCountFlagBits,
                          const VkClearColorValue &clearColor,
                          VkBuffer vertices, VkBuffer indices,
                          uint32_t drawCount, const XrPosef &hmdPose,
                          XrFovf fov, const std::vector<Cube> &cubes) {
  auto found = this->framebufferMap.find(image);
  if (found == this->framebufferMap.end()) {

    auto rt = std::make_shared<vuloxr::vk::SwapchainFramebuffer>(
        physicalDevice, this->device, image, extent, colorFormat,
        this->pipeline.renderPass, depthFormat, sampleCountFlagBits);
    found = this->framebufferMap.insert({image, rt}).first;

    {
      // once record
      // after use vkCmdPushConstants
      vuloxr::vk::CommandScope(this->commandBuffer)
          .transitionDepthLayout(
              rt->depth.image, VK_IMAGE_LAYOUT_UNDEFINED,
              VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
      VkSubmitInfo submitInfo{
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .commandBufferCount = 1,
          .pCommandBuffers = &this->commandBuffer,
      };
      vuloxr::vk::CheckVkResult(
          vkQueueSubmit(this->queue, 1, &submitInfo, nullptr));
      vkQueueWaitIdle(this->queue);
    }
  }

  // Waiting on a not-in-flight command buffer is a no-op
  execFence.wait();
  execFence.reset();

  vuloxr::vk::CheckVkResult(vkResetCommandBuffer(this->commandBuffer, 0));

  VkClearValue clearValues[] = {
      {
          .color = clearColor,
      },
      {
          .depthStencil =
              {
                  .depth = 1.0f,
                  .stencil = 0,
              },
      },
  };

  {
    vuloxr::vk::RenderPassRecording recording(
        this->commandBuffer, this->pipeline.pipelineLayout,
        this->pipeline.renderPass, found->second->framebuffer, extent,
        clearValues, std::size(clearValues));

    vkCmdBindPipeline(this->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      this->pipeline);

    // Bind index and vertex buffers
    vkCmdBindIndexBuffer(this->commandBuffer, indices, 0, VK_INDEX_TYPE_UINT16);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(this->commandBuffer, 0, 1, &vertices, &offset);

    // Render each cube
    for (auto &cube : cubes) {
      // Compute the view-projection transform. Note all matrixes
      // (including OpenXR's) are column-major, right-handed.
      XrMatrix4x4f proj;
      XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_VULKAN, fov, 0.05f,
                                       100.0f);
      XrMatrix4x4f toView;
      XrMatrix4x4f_CreateFromRigidTransform(&toView, &hmdPose);
      XrMatrix4x4f view;
      XrMatrix4x4f_InvertRigidBody(&view, &toView);
      XrMatrix4x4f vp;
      XrMatrix4x4f_Multiply(&vp, &proj, &view);

      // Compute the model-view-projection transform and push it.
      XrMatrix4x4f model;
      XrMatrix4x4f_CreateTranslationRotationScale(
          &model, &cube.pose.position, &cube.pose.orientation, &cube.scale);
      XrMatrix4x4f mvp;
      XrMatrix4x4f_Multiply(&mvp, &vp, &model);
      vkCmdPushConstants(this->commandBuffer, this->pipeline.pipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvp), &mvp.m[0]);

      // Draw the cube.
      vkCmdDrawIndexed(this->commandBuffer, drawCount, 1, 0, 0, 0);
    }
  }

  VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &this->commandBuffer,
  };
  vuloxr::vk::CheckVkResult(
      vkQueueSubmit(this->queue, 1, &submitInfo, execFence));
}

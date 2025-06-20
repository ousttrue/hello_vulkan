#include "../main_loop.h"
#include "pipeline.hpp"
#include "vuloxr/vk/pipeline.h"

void main_loop(const std::function<bool()> &runLoop,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *) {

  //
  // RenderPass
  //
  // Finally, create the renderpass.
  VkAttachmentDescription attachment = {
      // Backbuffer format.
      .format = swapchain.createInfo.imageFormat,
      // Not multisampled.
      .samples = VK_SAMPLE_COUNT_1_BIT,
      // When starting the frame, we want tiles to be cleared.
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      // When ending the frame, we want tiles to be written out.
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      // Don't care about stencil since we're not using it.
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

      // The image layout will be undefined when the render pass begins.
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      // After the render pass is complete, we will transition to
      // PRESENT_SRC_KHR
      // layout.
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference colorRef = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorRef,
  };

  VkSubpassDependency dependency = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      // Since we changed the image layout, we need to make the memory visible
      // to color attachment to modify.
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };

  VkRenderPassCreateInfo rpInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &attachment,
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 1,
      .pDependencies = &dependency,
  };
  VkRenderPass renderPass;
  vuloxr::vk::CheckVkResult(
      vkCreateRenderPass(device, &rpInfo, nullptr, &renderPass));

  auto pipelineLayout = vuloxr::vk::createEmptyPipelineLayout(device);

  auto pipeline = Pipeline::create(physicalDevice.physicalDevice, device,
                                   renderPass, pipelineLayout);

  std::vector<std::shared_ptr<vuloxr::vk::SwapchainFramebuffer>> backbuffers(
      swapchain.images.size());
  vuloxr::vk::FlightManager flightManager(
      device, physicalDevice.graphicsFamilyIndex, swapchain.images.size());

  vuloxr::FrameCounter counter;
  while (runLoop()) {
    auto acquireSemaphore = flightManager.getOrCreateSemaphore();
    auto acquired = swapchain.acquireNextImage(acquireSemaphore);
    if (acquired.result == VK_SUCCESS) {
      auto backbuffer = backbuffers[acquired.imageIndex];
      if (!backbuffer) {
        backbuffer = std::make_shared<vuloxr::vk::SwapchainFramebuffer>(
            device, acquired.image, swapchain.createInfo.imageExtent,
            swapchain.createInfo.imageFormat, renderPass);
        backbuffers[acquired.imageIndex] = backbuffer;
      }

      // All queue submissions get a fence that CPU will wait
      // on for synchronization purposes.
      auto [cmd, flight] = flightManager.sync(acquireSemaphore);

      {
        // We will only submit this once before it's recycled.
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd, &beginInfo);

        // Set clear color values.
        VkClearValue clearValue{
            .color =
                {
                    .float32 =
                        {
                            0.1f,
                            0.1f,
                            0.2f,
                            1.0f,
                        },
                },
        };

        // Begin the render pass.
        VkRenderPassBeginInfo rpBegin = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderPass,
            .framebuffer = backbuffer->framebuffer,
            .renderArea = {.extent = swapchain.createInfo.imageExtent},
            .clearValueCount = 1,
            .pClearValues = &clearValue,
        };

        // We will add draw commands in the same command buffer.
        vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

        // Signal the underlying context that we're using this backbuffer now.
        // This will also wait for all fences associated with this swapchain
        // image to complete first. When submitting command buffer that writes
        // to swapchain, we need to wait for this semaphore first. Also, delete
        // the older semaphore. auto oldSemaphore = backbuffer->beginFrame(cmd,
        // acquireSemaphore);
        pipeline->render(cmd, backbuffer->framebuffer,
                         swapchain.createInfo.imageExtent);

        // Complete render pass.
        vkCmdEndRenderPass(cmd);

        // Complete the command buffer.
        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
          vuloxr::Logger::Error("vkEndCommandBuffer");
          abort();
        }
      }

      const VkPipelineStageFlags waitStage =
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      VkSubmitInfo info = {
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .waitSemaphoreCount =
              static_cast<uint32_t>(acquireSemaphore != VK_NULL_HANDLE ? 1 : 0),
          .pWaitSemaphores = &acquireSemaphore,
          .pWaitDstStageMask = &waitStage,
          .commandBufferCount = 1,
          .pCommandBuffers = &cmd,
          .signalSemaphoreCount = 1,
          .pSignalSemaphores = &flight.submitSemaphore,
      };
      vuloxr::vk::CheckVkResult(
          vkQueueSubmit(device.queue, 1, &info, flight.submitFence));

      VkPresentInfoKHR present = {
          .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = &flight.submitSemaphore,
          .swapchainCount = 1,
          .pSwapchains = &swapchain.swapchain,
          .pImageIndices = &acquired.imageIndex,
          .pResults = nullptr,
      };
      vuloxr::vk::CheckVkResult(vkQueuePresentKHR(device.queue, &present));

    } else if (acquired.result == VK_SUBOPTIMAL_KHR ||
               acquired.result == VK_ERROR_OUT_OF_DATE_KHR) {
      vuloxr::Logger::Error("[RESULT_ERROR_OUTDATED_SWAPCHAIN]");
      vkQueueWaitIdle(swapchain.presentQueue);
      flightManager.reuseSemaphore(acquireSemaphore);
      // TODO:
      // swapchain = {};
      // return true;

    } else {
      // error ?
      vuloxr::Logger::Error("Unrecoverable swapchain error.");
      vkQueueWaitIdle(swapchain.presentQueue);
      flightManager.reuseSemaphore(acquireSemaphore);
      return;
    }

    counter.frameEnd();
  }

  vkDeviceWaitIdle(device);
}

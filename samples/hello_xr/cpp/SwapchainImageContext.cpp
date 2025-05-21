#include "SwapchainImageContext.h"
#include "DepthBuffer.h"
#include "Pipeline.h"
#include "RenderPass.h"
#include "RenderTarget.h"
#include <vulkan/vulkan_core.h>

std::shared_ptr<SwapchainImageContext> SwapchainImageContext::Create(
    VkDevice device, const std::shared_ptr<class MemoryAllocator> &memAllocator,
    uint32_t capacity, VkExtent2D size, VkFormat format,
    VkSampleCountFlagBits sampleCount,
    const std::shared_ptr<class PipelineLayout> &layout,
    const std::shared_ptr<class ShaderProgram> &sp,
    const std::shared_ptr<VertexBuffer> &vb) {

  auto ptr = std::shared_ptr<SwapchainImageContext>(new SwapchainImageContext);
  ptr->m_vkDevice = device;

  ptr->m_size = size;
  VkFormat colorFormat = format;
  VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
  // XXX handle swapchainCreateInfo.sampleCount

  ptr->m_depthBuffer = DepthBuffer::Create(ptr->m_vkDevice, memAllocator, size,
                                           depthFormat, sampleCount);
  ptr->m_rp = RenderPass::Create(ptr->m_vkDevice, colorFormat, depthFormat);
  ptr->m_pipe =
      Pipeline::Create(ptr->m_vkDevice, ptr->m_size, layout, ptr->m_rp, sp, vb);

  ptr->m_bases.resize(capacity);
  ptr->m_swapchainImages.resize(capacity);
  ptr->m_renderTarget.resize(capacity);
  for (uint32_t i = 0; i < capacity; ++i) {
    ptr->m_swapchainImages[i] = {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR};
    ptr->m_bases[i] = reinterpret_cast<XrSwapchainImageBaseHeader *>(
        &ptr->m_swapchainImages[i]);
  }

  return ptr;
}

void SwapchainImageContext::BindRenderTarget(
    uint32_t index, VkRenderPassBeginInfo *renderPassBeginInfo) {
  if (!m_renderTarget[index]) {
    m_renderTarget[index] =
        RenderTarget::Create(m_vkDevice, m_swapchainImages[index].image,
                             m_depthBuffer->depthImage, m_size, m_rp);
  }
  renderPassBeginInfo->renderPass = m_rp->pass;
  renderPassBeginInfo->framebuffer = m_renderTarget[index]->fb;
  renderPassBeginInfo->renderArea.offset = {0, 0};
  renderPassBeginInfo->renderArea.extent = m_size;
}

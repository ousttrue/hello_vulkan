#include "SwapchainImageContext.h"
#include "DepthBuffer.h"
#include "Pipeline.h"
#include "RenderPass.h"
#include "RenderTarget.h"
#include <vulkan/vulkan_core.h>

std::vector<XrSwapchainImageBaseHeader *> SwapchainImageContext::Create(
    VkDevice device, const std::shared_ptr<class MemoryAllocator> &memAllocator,
    uint32_t capacity, VkExtent2D size, VkFormat format,
    VkSampleCountFlagBits sampleCount,
    const std::shared_ptr<class PipelineLayout> &layout,
    const std::shared_ptr<class ShaderProgram> &sp,
    const std::shared_ptr<VertexBuffer> &vb) {
  m_vkDevice = device;

  m_size = size;
  VkFormat colorFormat = format;
  VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
  // XXX handle swapchainCreateInfo.sampleCount

  depthBuffer = DepthBuffer::Create(m_vkDevice, memAllocator, size, depthFormat,
                                    sampleCount);
  m_rp = RenderPass::Create(m_vkDevice, colorFormat, depthFormat);
  m_pipe = Pipeline::Create(m_vkDevice, m_size, layout, m_rp, sp, vb);

  swapchainImages.resize(capacity);
  renderTarget.resize(capacity);
  std::vector<XrSwapchainImageBaseHeader *> bases(capacity);
  for (uint32_t i = 0; i < capacity; ++i) {
    swapchainImages[i] = {swapchainImageType};
    bases[i] =
        reinterpret_cast<XrSwapchainImageBaseHeader *>(&swapchainImages[i]);
  }

  return bases;
}

void SwapchainImageContext::BindRenderTarget(
    uint32_t index, VkRenderPassBeginInfo *renderPassBeginInfo) {
  if (!renderTarget[index]) {
    renderTarget[index] =
        RenderTarget::Create(m_vkDevice, swapchainImages[index].image,
                             depthBuffer->depthImage, m_size, m_rp);
  }
  renderPassBeginInfo->renderPass = m_rp->pass;
  renderPassBeginInfo->framebuffer = renderTarget[index]->fb;
  renderPassBeginInfo->renderArea.offset = {0, 0};
  renderPassBeginInfo->renderArea.extent = m_size;
}

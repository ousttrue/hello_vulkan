#include "VulkanRenderer.h"
#include "DepthBuffer.h"
#include "Pipeline.h"
#include "RenderPass.h"
#include "RenderTarget.h"
#include "VertexBuffer.h"
#include <vulkan/vulkan_core.h>

std::shared_ptr<VulkanRenderer> VulkanRenderer::Create(
    VkDevice device, const std::shared_ptr<class MemoryAllocator> &memAllocator,
    VkExtent2D size, VkFormat format, VkSampleCountFlagBits sampleCount,
    const std::shared_ptr<class ShaderProgram> &sp) {

  auto ptr = std::shared_ptr<VulkanRenderer>(new VulkanRenderer);
  ptr->m_vkDevice = device;

  ptr->m_size = size;
  VkFormat colorFormat = format;
  VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
  // XXX handle swapchainCreateInfo.sampleCount

  ptr->m_pipelineLayout = PipelineLayout::Create(ptr->m_vkDevice);
  static_assert(sizeof(Vertex) == 24, "Unexpected Vertex size");
  ptr->m_drawBuffer = VertexBuffer::Create(
      ptr->m_vkDevice, memAllocator,
      {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position)},
       {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Color)}},
      c_cubeVertices, std::size(c_cubeVertices), c_cubeIndices,
      std::size(c_cubeIndices));

  ptr->m_depthBuffer = DepthBuffer::Create(ptr->m_vkDevice, memAllocator, size,
                                           depthFormat, sampleCount);
  ptr->m_rp = RenderPass::Create(ptr->m_vkDevice, colorFormat, depthFormat);
  ptr->m_pipe =
      Pipeline::Create(ptr->m_vkDevice, ptr->m_size, ptr->m_pipelineLayout,
                       ptr->m_rp, sp, ptr->m_drawBuffer);

  // ptr->m_bases.resize(capacity);
  // ptr->m_swapchainImages.resize(capacity);
  // for (uint32_t i = 0; i < capacity; ++i) {
  //   ptr->m_swapchainImages[i] = {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR};
  //   ptr->m_bases[i] = reinterpret_cast<XrSwapchainImageBaseHeader *>(
  //       &ptr->m_swapchainImages[i]);
  // }

  return ptr;
}

void VulkanRenderer::RenderView(VkCommandBuffer cmd, VkImage image,
                                const Vec4 &clearColor,
                                const std::vector<Mat4> &cubes) {

  // Ensure depth is in the right layout
  m_depthBuffer->TransitionLayout(
      cmd, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  // Bind and clear eye render target
  static std::array<VkClearValue, 2> clearValues;
  clearValues[0].color.float32[0] = clearColor.x;
  clearValues[0].color.float32[1] = clearColor.y;
  clearValues[0].color.float32[2] = clearColor.z;
  clearValues[0].color.float32[3] = clearColor.w;
  clearValues[1].depthStencil.depth = 1.0f;
  clearValues[1].depthStencil.stencil = 0;
  VkRenderPassBeginInfo renderPassBeginInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .clearValueCount = static_cast<uint32_t>(clearValues.size()),
      .pClearValues = clearValues.data(),
  };

  // void VulkanRenderer::BindRenderTarget(
  //     VkImage image, VkRenderPassBeginInfo *renderPassBeginInfo) {
  // }
  // BindRenderTarget(image, &renderPassBeginInfo);
  auto found = m_renderTarget.find(image);
  if (found == m_renderTarget.end()) {
    auto rt = RenderTarget::Create(m_vkDevice, image, m_depthBuffer->depthImage,
                                   m_size, m_rp);
    found = m_renderTarget.insert({image, rt}).first;
  }
  renderPassBeginInfo.renderPass = m_rp->pass;
  renderPassBeginInfo.framebuffer = found->second->fb;
  renderPassBeginInfo.renderArea.offset = {0, 0};
  renderPassBeginInfo.renderArea.extent = m_size;

  vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipe->pipe);

  // Bind index and vertex buffers
  vkCmdBindIndexBuffer(cmd, m_drawBuffer->idxBuf, 0, VK_INDEX_TYPE_UINT16);
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &m_drawBuffer->vtxBuf, &offset);

  // Render each cube
  for (const Mat4 &cube : cubes) {
    vkCmdPushConstants(cmd, m_pipelineLayout->layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(cube), &cube.m[0]);

    // Draw the cube.
    vkCmdDrawIndexed(cmd, m_drawBuffer->count.idx, 1, 0, 0, 0);
  }
}

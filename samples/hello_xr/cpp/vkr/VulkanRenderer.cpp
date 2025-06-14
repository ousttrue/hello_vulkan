#include "VulkanRenderer.h"
#include "DepthBuffer.h"
#include "MemoryAllocator.h"
#include "RenderTarget.h"
#include "VertexBuffer.h"
#include <common/fmt.h>
#include <common/logger.h>
#include <vko/vko.h>
#include <vulkan/vulkan_core.h>

VulkanRenderer::VulkanRenderer(VkPhysicalDevice physicalDevice, VkDevice device,
                               uint32_t queueFamilyIndex, VkExtent2D size,
                               VkFormat depthFormat,
                               VkSampleCountFlagBits sampleCount)
    : m_physicalDevice(physicalDevice), m_device(device),
      m_queueFamilyIndex(queueFamilyIndex) {

  m_memAllocator = MemoryAllocator::Create(m_physicalDevice, m_device);

  static_assert(sizeof(Vertex) == 24, "Unexpected Vertex size");
  m_drawBuffer = VertexBuffer::Create(
      m_device, m_memAllocator,
      {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position)},
       {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Color)}},
      c_cubeVertices, std::size(c_cubeVertices), c_cubeIndices,
      std::size(c_cubeIndices));

  m_depthBuffer = DepthBuffer::Create(m_device, m_memAllocator, size,
                                      depthFormat, sampleCount);
}

void VulkanRenderer::RenderView(VkCommandBuffer cmd, VkRenderPass renderPass,
                                VkPipelineLayout pipelineLayout,
                                VkPipeline pipeline, VkImage image,
                                VkExtent2D size, VkFormat colorFormat, VkFormat depthFormat,
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
    auto rt =
        RenderTarget::Create(m_device, image, m_depthBuffer->depthImage, size,
                             colorFormat, depthFormat, renderPass);
    found = m_renderTarget.insert({image, rt}).first;
  }
  renderPassBeginInfo.renderPass = renderPass;
  renderPassBeginInfo.framebuffer = found->second->fb;
  renderPassBeginInfo.renderArea.offset = {0, 0};
  renderPassBeginInfo.renderArea.extent = size;

  vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  // Bind index and vertex buffers
  vkCmdBindIndexBuffer(cmd, m_drawBuffer->idxBuf, 0, VK_INDEX_TYPE_UINT16);
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &m_drawBuffer->vtxBuf, &offset);

  // Render each cube
  for (const Mat4 &cube : cubes) {
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(cube), &cube.m[0]);

    // Draw the cube.
    vkCmdDrawIndexed(cmd, m_drawBuffer->count.idx, 1, 0, 0, 0);
  }
}

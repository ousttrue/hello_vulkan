#include "VulkanRenderer.h"
#include "CmdBuffer.h"
#include "DepthBuffer.h"
#include "MemoryAllocator.h"
#include "Pipeline.h"
#include "RenderPass.h"
#include "RenderTarget.h"
#include "VertexBuffer.h"
#include "../logger.h"
#include "../fmt.h"
#include "vulkan_debug_object_namer.hpp"
#include <vulkan/vulkan_core.h>

#include <shaderc/shaderc.hpp>
// Compile a shader to a SPIR-V binary.
static std::vector<uint32_t> CompileGlslShader(const std::string &name,
                                               shaderc_shader_kind kind,
                                               const std::string &source) {
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;

  options.SetOptimizationLevel(shaderc_optimization_level_size);

  shaderc::SpvCompilationResult module =
      compiler.CompileGlslToSpv(source, kind, name.c_str(), options);

  if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
    Log::Write(Log::Level::Error,
               Fmt("Shader %s compilation failed: %s", name.c_str(),
                   module.GetErrorMessage().c_str()));
    return std::vector<uint32_t>();
  }

  return {module.cbegin(), module.cend()};
}

constexpr char VertexShaderGlsl[] = R"_(
#version 430
#extension GL_ARB_separate_shader_objects : enable

layout (std140, push_constant) uniform buf
{
    mat4 mvp;
} ubuf;

layout (location = 0) in vec3 Position;
layout (location = 1) in vec4 Color;

layout (location = 0) out vec4 oColor;
out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    oColor.rgba  = Color.rgba;
    gl_Position = ubuf.mvp * vec4(Position, 1);
}
)_";

constexpr char FragmentShaderGlsl[] = R"_(
#version 430
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec4 oColor;

layout (location = 0) out vec4 FragColor;

void main()
{
    FragColor = oColor;
}
)_";

// glslangValidator doesn't wrap its output in brackets if you don't have it
// define the whole array.
#if defined(USE_GLSLANGVALIDATOR)
#define SPV_PREFIX {
#define SPV_SUFFIX }
#else
#define SPV_PREFIX
#define SPV_SUFFIX
#endif

VulkanRenderer::VulkanRenderer(VkPhysicalDevice physicalDevice, VkDevice device,
                               uint32_t queueFamilyIndex, VkExtent2D size,
                               VkFormat format,
                               VkSampleCountFlagBits sampleCount)
    : m_physicalDevice(physicalDevice), m_device(device),
      m_queueFamilyIndex(queueFamilyIndex) {
  vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);

  m_memAllocator = MemoryAllocator::Create(m_physicalDevice, m_device);

  m_size = size;
  VkFormat colorFormat = format;
  VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
  // XXX handle swapchainCreateInfo.sampleCount

  auto vertexSPIRV = CompileGlslShader(
      "vertex", shaderc_glsl_default_vertex_shader, VertexShaderGlsl);
  auto fragmentSPIRV = CompileGlslShader(
      "fragment", shaderc_glsl_default_fragment_shader, FragmentShaderGlsl);
  if (vertexSPIRV.empty()) {
    throw std::runtime_error("Failed to compile vertex shader");
  }
  if (fragmentSPIRV.empty()) {
    throw std::runtime_error("Failed to compile fragment shader");
  }

  m_shaderProgram = ShaderProgram::Create(m_device);
  m_shaderProgram->LoadVertexShader(vertexSPIRV);
  m_shaderProgram->LoadFragmentShader(fragmentSPIRV);

  // Semaphore to block on draw complete
  VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  if (vkCreateSemaphore(m_device, &semInfo, nullptr, &m_vkDrawDone) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateSemaphore");
  }
  if (SetDebugUtilsObjectNameEXT(
          m_device, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)m_vkDrawDone,
          "hello_xr draw done semaphore") != VK_SUCCESS) {
    throw std::runtime_error("SetDebugUtilsObjectNameEXT");
  }

  m_cmdBuffer = CmdBuffer::Create(m_device, m_queueFamilyIndex);
  if (!m_cmdBuffer) {
    throw std::runtime_error("Failed to create command buffer");
  }

#if defined(USE_MIRROR_WINDOW)
  m_swapchain.Create(m_vkInstance, m_vkPhysicalDevice, m_vkDevice,
                     m_queueFamilyIndex);
  m_cmdBuffer.Reset();
  m_cmdBuffer.Begin();
  m_swapchain.Prepare(m_cmdBuffer.buf);
  m_cmdBuffer.End();
  m_cmdBuffer.Exec(m_vkQueue);
  m_cmdBuffer.Wait();
#endif

  m_pipelineLayout = PipelineLayout::Create(m_device);
  static_assert(sizeof(Vertex) == 24, "Unexpected Vertex size");
  m_drawBuffer = VertexBuffer::Create(
      m_device, m_memAllocator,
      {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position)},
       {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Color)}},
      c_cubeVertices, std::size(c_cubeVertices), c_cubeIndices,
      std::size(c_cubeIndices));

  m_depthBuffer = DepthBuffer::Create(m_device, m_memAllocator, size,
                                      depthFormat, sampleCount);
  m_rp = RenderPass::Create(m_device, colorFormat, depthFormat);
  m_pipe = Pipeline::Create(m_device, m_size, m_pipelineLayout, m_rp,
                            m_shaderProgram, m_drawBuffer);
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
    auto rt = RenderTarget::Create(m_device, image, m_depthBuffer->depthImage,
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

VkCommandBuffer VulkanRenderer::BeginCommand() {
  // CHECK(layerView.subImage.imageArrayIndex ==
  //       0); // Texture arrays not supported.

  // XXX Should double-buffer the command buffers, for now just flush
  m_cmdBuffer->Wait();
  m_cmdBuffer->Reset();
  m_cmdBuffer->Begin();
  return m_cmdBuffer->buf;
}

void VulkanRenderer::EndCommand(VkCommandBuffer cmd) {
  vkCmdEndRenderPass(cmd);
  m_cmdBuffer->End();
  m_cmdBuffer->Exec(m_queue);

#if defined(USE_MIRROR_WINDOW)
  // Cycle the window's swapchain on the last view rendered
  if (swapchainContext == &m_swapchainImageContexts.back()) {
    m_swapchain.Acquire();
    m_swapchain.Wait();
    m_swapchain.Present(m_vkQueue);
  }
#endif
}

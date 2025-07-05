#include "../xr_main_loop.h"
#include <vuloxr/vk/pipeline.h>
#include <vuloxr/vk/shaderc.h>

auto DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

struct Impl {
  const Graphics *graphics;
  std::shared_ptr<GraphicsSwapchain> swapchain;

  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  VkCommandPool commandPool = VK_NULL_HANDLE;

  vuloxr::vk::VertexBuffer vertices;
  vuloxr::vk::IndexBuffer indices;
  vuloxr::vk::Pipeline pipeline;

  struct RenderTarget {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vuloxr::vk::Fence execFence;
  };
  std::vector<std::shared_ptr<RenderTarget>> renderTargets;
  vuloxr::vk::SwapchainIsolatedDepthFramebufferList framebuffers;

  Impl(const Graphics *g, const std::shared_ptr<GraphicsSwapchain> &_swapchain)
      : graphics(g), swapchain(_swapchain), device(g->device),
        framebuffers(
            g->device, (VkFormat)swapchain->swapchainCreateInfo.format,
            DEPTH_FORMAT,
            (VkSampleCountFlagBits)swapchain->swapchainCreateInfo.sampleCount) {
    vkGetDeviceQueue(this->device,
                     this->graphics->physicalDevice.graphicsFamilyIndex, 0,
                     &this->queue);

    VkCommandPoolCreateInfo cmdPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = this->graphics->physicalDevice.graphicsFamilyIndex,
    };
    vuloxr::vk::CheckVkResult(vkCreateCommandPool(this->device, &cmdPoolInfo,
                                                  nullptr, &this->commandPool));
  }

  ~Impl() {
    if (this->commandPool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(this->device, this->commandPool, nullptr);
    }
  }

  void initScene(const char *vertexShaderGlsl, const char *fragmentShaderGlsl,
                 std::span<const VertexAttributeLayout> layouts,
                 const InputData &vertices, const InputData &indices) {
    this->vertices.bindings = {
        {
            .binding = 0,
            .stride = vertices.stride,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
    };
    for (uint32_t i = 0; i < layouts.size(); ++i) {
      this->vertices.attributes.push_back({
          .location = i,
          .binding = 0,
          .format = vuloxr::vk::getFloatFormat(layouts[i].components),
          .offset = layouts[i].offset,
      });
    }
    this->vertices.allocate(this->graphics->physicalDevice,
                            this->graphics->device, vertices.data,
                            vertices.byteSize(), vertices.drawCount);

    this->indices.allocate(this->graphics->physicalDevice,
                           this->graphics->device, indices.data,
                           indices.byteSize(), indices.drawCount);

    // pieline
    auto pipelineLayout = vuloxr::vk::createPipelineLayoutWithConstantSize(
        this->graphics->device, sizeof(float) * 16);
    auto [renderPass, depthStencil] = vuloxr::vk::createColorDepthRenderPass(
        this->graphics->device,
        (VkFormat)this->swapchain->swapchainCreateInfo.format, DEPTH_FORMAT);

    auto vertexSPIRV = vuloxr::vk::glsl_vs_to_spv(vertexShaderGlsl);
    assert(vertexSPIRV.size());
    auto vs = vuloxr::vk::ShaderModule::createVertexShader(
        this->graphics->device, vertexSPIRV, "main");

    auto fragmentSPIRV = vuloxr::vk::glsl_fs_to_spv(fragmentShaderGlsl);
    assert(fragmentSPIRV.size());
    auto fs = vuloxr::vk::ShaderModule::createFragmentShader(
        this->graphics->device, fragmentSPIRV, "main");

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        vs.pipelineShaderStageCreateInfo,
        fs.pipelineShaderStageCreateInfo,
    };

    vuloxr::vk::PipelineBuilder builder;
    this->pipeline = builder.create(
        this->graphics->device, renderPass, depthStencil, pipelineLayout,
        stages, this->vertices.bindings, this->vertices.attributes, {}, {},
        {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
  }

  void initSwapchain(int width, int height,
                     std::span<const SwapchainImageType> images) {
    std::vector<VkImage> vkImages;
    for (auto &image : images) {
      vkImages.push_back(image.image);
    }
    framebuffers.reset(this->graphics->physicalDevice,
                       this->pipeline.renderPass,
                       VkExtent2D{uint32_t(width), uint32_t(height)}, vkImages);

    this->renderTargets.resize(images.size());
    for (int index = 0; index < images.size(); ++index) {
      auto rt = std::make_shared<RenderTarget>();

      rt->execFence = vuloxr::vk::Fence(this->device, true);

      VkCommandBufferAllocateInfo cmd{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .commandPool = this->commandPool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = 1,
      };
      vuloxr::vk::CheckVkResult(
          vkAllocateCommandBuffers(this->device, &cmd, &rt->commandBuffer));

      this->renderTargets[index] = rt;
    }
  }

  void render(uint32_t index, const XrColor4f &clearColor,
              std::span<const DirectX::XMFLOAT4X4> matrices) {
    auto framebuffer = &this->framebuffers[index];
    auto rt = this->renderTargets[index];

    // Waiting on a not-in-flight command buffer is a no-op
    rt->execFence.wait();
    rt->execFence.reset();

    vuloxr::vk::CheckVkResult(vkResetCommandBuffer(rt->commandBuffer, 0));

    {
      VkExtent2D extent{
          this->swapchain->swapchainCreateInfo.width,
          this->swapchain->swapchainCreateInfo.height,
      };
      VkClearValue clearValues[] = {
          {.color = {clearColor.r, clearColor.g, clearColor.b, clearColor.a}},
          {.depthStencil = {.depth = 1.0f, .stencil = 0}},
      };

      vuloxr::vk::RenderPassRecording recording(
          rt->commandBuffer, this->pipeline.pipelineLayout,
          this->pipeline.renderPass, framebuffer->framebuffer, extent,
          clearValues);

      vkCmdBindPipeline(rt->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pipeline);

      // Bind index and vertex buffers
      vkCmdBindIndexBuffer(rt->commandBuffer, indices.buffer, 0,
                           VK_INDEX_TYPE_UINT16);
      VkDeviceSize offset = 0;
      vkCmdBindVertexBuffers(rt->commandBuffer, 0, 1, &vertices.buffer.buffer,
                             &offset);

      // Render each cube
      for (auto &m : matrices) {
        vkCmdPushConstants(rt->commandBuffer, this->pipeline.pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(m), &m.m);

        // Draw the cube.
        vkCmdDrawIndexed(rt->commandBuffer, this->indices.drawCount, 1, 0, 0,
                         0);
      }
    }

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &rt->commandBuffer,
    };
    vuloxr::vk::CheckVkResult(
        vkQueueSubmit(this->queue, 1, &submitInfo, rt->execFence));
  }
};

ViewRenderer::ViewRenderer(const Graphics *g,
                           const std::shared_ptr<GraphicsSwapchain> &swapchain)
    : _impl(new Impl(g, swapchain)) {}

ViewRenderer::~ViewRenderer() { delete this->_impl; }

void ViewRenderer::initScene(const char *vs, const char *fs,
                             std::span<const VertexAttributeLayout> layouts,
                             const InputData &vertices,
                             const InputData &indices) {
  this->_impl->initScene(vs, fs, layouts, vertices, indices);
}
void ViewRenderer::initSwapchain(int width, int height,
                                 std::span<const SwapchainImageType> images) {
  this->_impl->initSwapchain(width, height, images);
}
void ViewRenderer::render(uint32_t index, const XrColor4f &clearColor,
                          std::span<const DirectX::XMFLOAT4X4> matrices) {
  this->_impl->render(index, clearColor, matrices);
}

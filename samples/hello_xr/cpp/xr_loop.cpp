#include "xr_loop.h"
#include "openxr_program/CubeScene.h"
#include "openxr_program/openxr_swapchain.h"
#include "openxr_program/options.h"
#include "vko/vko.h"
#include "vkr/DepthBuffer.h"
#include "vkr/Pipeline.h"
#include "vkr/RenderTarget.h"
#include "vkr/VertexBuffer.h"
#include <map>

struct ViewRenderer {
  VkDevice device;
  VkQueue queue;
  vko::Fence execFence;
  VkCommandPool commandPool = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

  std::shared_ptr<Pipeline> m_pipeline;

  std::shared_ptr<struct VertexBuffer> m_drawBuffer;
  std::shared_ptr<class DepthBuffer> m_depthBuffer;
  std::map<VkImage, std::shared_ptr<class RenderTarget>> m_renderTarget;

  ViewRenderer(VkPhysicalDevice physicalDevice, VkDevice _device,
               uint32_t queueFamilyIndex, VkExtent2D extent,
               VkFormat colorFormat, VkFormat depthFormat,
               VkSampleCountFlagBits sampleCountFlagBits)
      : device(_device), execFence(_device, true) {
    vkGetDeviceQueue(this->device, queueFamilyIndex, 0, &this->queue);

    static_assert(sizeof(Vertex) == 24, "Unexpected Vertex size");
    m_drawBuffer = VertexBuffer::Create(
        this->device, physicalDevice,
        {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position)},
         {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Color)}},
        c_cubeVertices, std::size(c_cubeVertices), c_cubeIndices,
        std::size(c_cubeIndices));

    m_depthBuffer = DepthBuffer::Create(this->device, physicalDevice, extent,
                                        depthFormat, sampleCountFlagBits);

    m_pipeline = Pipeline::Create(this->device, extent, colorFormat,
                                  depthFormat, this->m_drawBuffer);

    VkCommandPoolCreateInfo cmdPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };
    VKO_CHECK(vkCreateCommandPool(this->device, &cmdPoolInfo, nullptr,
                                  &this->commandPool));
    if (SetDebugUtilsObjectNameEXT(device, VK_OBJECT_TYPE_COMMAND_POOL,
                                   (uint64_t)this->commandPool,
                                   "hello_xr command pool") != VK_SUCCESS) {
      throw std::runtime_error("SetDebugUtilsObjectNameEXT");
    }

    VkCommandBufferAllocateInfo cmd{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = this->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VKO_CHECK(
        vkAllocateCommandBuffers(this->device, &cmd, &this->commandBuffer));
    if (SetDebugUtilsObjectNameEXT(device, VK_OBJECT_TYPE_COMMAND_BUFFER,
                                   (uint64_t)this->commandBuffer,
                                   "hello_xr command buffer") != VK_SUCCESS) {
      throw std::runtime_error("SetDebugUtilsObjectNameEXT");
    }
  }

  ~ViewRenderer() {
    vkDestroyFence(this->device, execFence, nullptr);

    vkFreeCommandBuffers(this->device, this->commandPool, 1,
                         &this->commandBuffer);
    vkDestroyCommandPool(this->device, this->commandPool, nullptr);
  }

  void render(VkImage image, VkExtent2D size, VkFormat colorFormat,
              VkFormat depthFormat, const Vec4 &clearColor,
              const std::vector<Mat4> &matrices) {
    // Waiting on a not-in-flight command buffer is a no-op
    execFence.wait();
    execFence.reset();

    VKO_CHECK(vkResetCommandBuffer(this->commandBuffer, 0));

    VkCommandBufferBeginInfo cmdBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    VKO_CHECK(vkBeginCommandBuffer(this->commandBuffer, &cmdBeginInfo));

    // Ensure depth is in the right layout
    m_depthBuffer->TransitionLayout(
        this->commandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

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

    // BindRenderTarget(image, &renderPassBeginInfo);
    auto found = m_renderTarget.find(image);
    if (found == m_renderTarget.end()) {
      auto rt = RenderTarget::Create(
          this->device, image, m_depthBuffer->depthImage, size, colorFormat,
          depthFormat, m_pipeline->m_renderPass);
      found = m_renderTarget.insert({image, rt}).first;
    }
    renderPassBeginInfo.renderPass = m_pipeline->m_renderPass;
    renderPassBeginInfo.framebuffer = found->second->fb;
    renderPassBeginInfo.renderArea.offset = {0, 0};
    renderPassBeginInfo.renderArea.extent = size;

    vkCmdBeginRenderPass(this->commandBuffer, &renderPassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(this->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      this->m_pipeline->m_pipeline);

    // Bind index and vertex buffers
    vkCmdBindIndexBuffer(this->commandBuffer, m_drawBuffer->idxBuf, 0,
                         VK_INDEX_TYPE_UINT16);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(this->commandBuffer, 0, 1, &m_drawBuffer->vtxBuf,
                           &offset);

    // Render each cube
    for (const Mat4 &mat : matrices) {
      vkCmdPushConstants(this->commandBuffer,
                         this->m_pipeline->m_pipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat), &mat.m[0]);

      // Draw the cube.
      vkCmdDrawIndexed(this->commandBuffer, m_drawBuffer->count.idx, 1, 0, 0,
                       0);
    }

    vkCmdEndRenderPass(this->commandBuffer);
    VKO_CHECK(vkEndCommandBuffer(this->commandBuffer));

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &this->commandBuffer,
    };
    VKO_CHECK(vkQueueSubmit(this->queue, 1, &submitInfo, execFence));
  }
};

void xr_loop(const std::function<bool()> &runLoop, const Options &options,
             const std::shared_ptr<OpenXrSession> &session,
             VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex,
             VkDevice device) {

  // Create resources for each view.
  auto config = session->GetSwapchainConfiguration();
  std::vector<std::shared_ptr<OpenXrSwapchain>> swapchains;
  std::vector<std::shared_ptr<ViewRenderer>> views;

  auto depthFormat = VK_FORMAT_D32_SFLOAT;

  for (uint32_t i = 0; i < config.Views.size(); i++) {
    // XrSwapchain
    auto swapchain = OpenXrSwapchain::Create(session->m_session, i,
                                             config.Views[i], config.Format);
    swapchains.push_back(swapchain);

    auto ptr = std::make_shared<ViewRenderer>(
        physicalDevice, device, queueFamilyIndex, swapchain->extent(),
        swapchain->format(), depthFormat, swapchain->sampleCountFlagBits());
    views.push_back(ptr);
  }

  // mainloop
  while (runLoop()) {

    auto frameState = session->BeginFrame();
    LayerComposition composition(options.Parsed.EnvironmentBlendMode,
                                 session->m_appSpace);

    if (frameState.shouldRender == XR_TRUE) {
      uint32_t viewCountOutput;
      if (session->LocateView(
              session->m_appSpace, frameState.predictedDisplayTime,
              options.Parsed.ViewConfigType, &viewCountOutput)) {
        // XrCompositionLayerProjection

        // update scene
        CubeScene scene;
        scene.addSpaceCubes(session->m_appSpace,
                            frameState.predictedDisplayTime,
                            session->m_visualizedSpaces);
        scene.addHandCubes(session->m_appSpace, frameState.predictedDisplayTime,
                           session->m_input);

        for (uint32_t i = 0; i < viewCountOutput; ++i) {

          // XrCompositionLayerProjectionView(left / right)
          auto swapchain = swapchains[i];
          auto info = swapchain->AcquireSwapchain(session->m_views[i]);
          composition.pushView(info.CompositionLayer);

          views[i]->render(info.Image, swapchain->extent(), swapchain->format(),
                           depthFormat, options.GetBackgroundClearColor(),
                           scene.CalcCubeMatrices(info.calcViewProjection()));

          swapchain->EndSwapchain();
        }
      }
    }

    // std::vector<XrCompositionLayerBaseHeader *>
    auto &layers = composition.commitLayers();
    session->EndFrame(frameState.predictedDisplayTime, layers);
  }

  vkDeviceWaitIdle(device);
}

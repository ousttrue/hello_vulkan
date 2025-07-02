// https://github.com/LunarG/VulkanSamples/blob/master/API-Samples/15-draw_cube/15-draw_cube.cpp
#include "../main_loop.h"
#include "cube_data.h"
#include <DirectXMath.h>
#include <assert.h>
#include <string.h>
#include <vuloxr/vk.h>
#include <vuloxr/vk/buffer.h>
#include <vuloxr/vk/command.h>
#include <vuloxr/vk/pipeline.h>
#include <vuloxr/vk/shaderc.h>

struct Camera {
  DirectX::XMFLOAT4X4 projection;
  DirectX::XMFLOAT4X4 view;

  Camera() {}

  void setProjection(float fovYDegrees, int width, int height) {
    auto fov = DirectX::XMConvertToRadians(fovYDegrees);
    auto aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    DirectX::XMStoreFloat4x4(
        &this->projection,
        DirectX::XMMatrixPerspectiveFovRH(fov, aspectRatio, 0.1f, 100.0f));
  }

  void setView(const DirectX::XMFLOAT3 &eye, const DirectX::XMFLOAT3 &focus,
               const DirectX::XMFLOAT3 &up) {
    DirectX::XMStoreFloat4x4(
        &this->view, DirectX::XMMatrixLookAtRH(DirectX::XMLoadFloat3(&eye),
                                               DirectX::XMLoadFloat3(&focus),
                                               DirectX::XMLoadFloat3(&up)));
  }

  DirectX::XMFLOAT4X4 mvp() {
    auto Model = DirectX::XMMatrixIdentity();
    // Vulkan clip space has inverted Y and half Z.
    auto Clip = DirectX::XMMatrixSet(1.0f, 0.0f, 0.0f, 0.0f,  //
                                     0.0f, -1.0f, 0.0f, 0.0f, //
                                     0.0f, 0.0f, 0.5f, 0.0f,  //
                                     0.0f, 0.0f, 0.5f, 1.0f);
    auto m = Model * DirectX::XMLoadFloat4x4(&this->view) *
             DirectX::XMLoadFloat4x4(&this->projection) * Clip;
    DirectX::XMFLOAT4X4 out;
    DirectX::XMStoreFloat4x4(&out, m);
    return out;
  }
};

VkClearValue clear_values[2] = {
    {.color = {0.2f, 0.2f, 0.2f, 0.2f}},
    {.depthStencil = {.depth = 1.0f, .stencil = 0}},
};

const char VS[] = {
#embed "15-draw_cube.vert"
    , 0, 0, 0, 0,
};
const char FS[] = {
#embed "15-draw_cube.frag"
    , 0, 0, 0, 0,
};

void main_loop(const vuloxr::gui::WindowLoopOnce &windowLoopOnce,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *window) {

  Camera camera;
  camera.setProjection(45.0f, swapchain.createInfo.imageExtent.width,
                       swapchain.createInfo.imageExtent.height);
  camera.setView({-5, 3, -10}, {0, 0, 0}, {0, 1, 0});

  vuloxr::vk::UniformBuffer<DirectX::XMFLOAT4X4> ubo(device);
  ubo.memory = physicalDevice.allocForMap(device, ubo.buffer);
  ubo.value = camera.mvp();
  ubo.mapWrite();

  vuloxr::vk::DescriptorSet descriptor(
      device, 1,
      {
          {
              .binding = 0,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
              .pImmutableSamplers = NULL,
          },
      });

  /* Now use the descriptor layout to create a pipeline layout */
  VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = NULL,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor.descriptorSetLayout,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = NULL,
  };
  VkPipelineLayout pipeline_layout;
  vuloxr::vk::CheckVkResult(vkCreatePipelineLayout(
      device, &pPipelineLayoutCreateInfo, NULL, &pipeline_layout));

  auto depth_format = physicalDevice.depthFormat();
  auto [render_pass, depthstencil] = vuloxr::vk::createColorDepthRenderPass(
      device, swapchain.createInfo.imageFormat, depth_format);

  auto vs = vuloxr::vk::ShaderModule::createVertexShader(
      device, vuloxr::vk::glsl_vs_to_spv(VS), "main");
  auto fs = vuloxr::vk::ShaderModule::createFragmentShader(
      device, vuloxr::vk::glsl_fs_to_spv(FS), "main");

  vuloxr::vk::VertexBuffer vertex_buffer{
      .bindings =
          {
              {
                  .binding = 0,
                  .stride = sizeof(Vertex),
                  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
              },
          },
      .attributes = {
          // describes position
          {
              .location = 0,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
              .offset = 0,
          },
          // describes color
          {
              .location = 1,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
              .offset = 16,
          },
      }};
  vertex_buffer.allocate(physicalDevice, device,
                         std::span<const Vertex>(g_vb_solid_face_colors_Data));

  descriptor.update(0, std::span<const vuloxr::vk::DescriptorUpdateInfo>({
                           vuloxr::vk::DescriptorUpdateInfo{
                               .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               .pBufferInfo = &ubo.info,
                           },
                       }));

  vuloxr::vk::PipelineBuilder builder;
  auto pipeline = builder.create(
      device, render_pass, depthstencil, pipeline_layout,
      {vs.pipelineShaderStageCreateInfo, fs.pipelineShaderStageCreateInfo},
      vertex_buffer.bindings, vertex_buffer.attributes, {}, {},
      {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

  vuloxr::vk::SwapchainSharedDepthFramebufferList framebuffers(
      device, swapchain.createInfo.imageFormat, depth_format,
      VK_SAMPLE_COUNT_1_BIT);
  framebuffers.reset(physicalDevice, render_pass,
                     swapchain.createInfo.imageExtent, swapchain.images);
  vuloxr::vk::CommandBufferPool pool(device,
                                     physicalDevice.graphicsFamilyIndex);
  pool.reset(1);

  vuloxr::vk::AcquireSemaphorePool semaphorePool(device);

  while (auto state = windowLoopOnce()) {
    auto acquireSemaphore = semaphorePool.getOrCreate();
    auto [res, acquired] = swapchain.acquireNextImage(acquireSemaphore);

    auto cmd = &pool[0];
    semaphorePool.resetFenceAndMakePairSemaphore(cmd->submitFence,
                                                 acquireSemaphore);

    {
      vuloxr::vk::RenderPassRecording recording(
          cmd->commandBuffer, pipeline_layout, render_pass,
          framebuffers[acquired.imageIndex].framebuffer,
          swapchain.createInfo.imageExtent, clear_values,
          descriptor.descriptorSets[0],
          VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

      vertex_buffer.draw(cmd->commandBuffer, pipeline);
    }

    device.submit(cmd->commandBuffer, acquireSemaphore, {}, cmd->submitFence);

    // Make sure command buffer is finished before presenting
    vuloxr::vk::CheckVkResult(cmd->waitFence(device));

    vuloxr::vk::CheckVkResult(
        swapchain.present(acquired.imageIndex /*, cmd->submitSemaphore*/));
  }

  vkDeviceWaitIdle(device);
}

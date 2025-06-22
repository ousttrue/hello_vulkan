#include "xr_vulkan_session.h"
#include "GetXrReferenceSpaceCreateInfo.h"
#include "xr_linear.h"
#include <map>
#include <thread>
#include <vko/vko.h>
#include <vko/vko_pipeline.h>
#include <vko/vko_shaderc.h>
#include <vulkan/vulkan_core.h>
#include <xro/xro.h>

char VertexShaderGlsl[] = {
#embed "shader.vert"
    , 0};

char FragmentShaderGlsl[] = {
#embed "shader.frag"
    , 0};

static_assert(sizeof(VertexShaderGlsl), "VertexShaderGlsl");
static_assert(sizeof(FragmentShaderGlsl), "FragmentShaderGlsl");

struct Vec3 {
  float x, y, z;
};

struct Mat4 {
  float m[16];
};

struct Vertex {
  Vec3 Position;
  Vec3 Color;
};

constexpr Vec3 Red{1, 0, 0};
constexpr Vec3 DarkRed{0.25f, 0, 0};
constexpr Vec3 Green{0, 1, 0};
constexpr Vec3 DarkGreen{0, 0.25f, 0};
constexpr Vec3 Blue{0, 0, 1};
constexpr Vec3 DarkBlue{0, 0, 0.25f};

// Vertices for a 1x1x1 meter cube. (Left/Right, Top/Bottom, Front/Back)
constexpr Vec3 LBB{-0.5f, -0.5f, -0.5f};
constexpr Vec3 LBF{-0.5f, -0.5f, 0.5f};
constexpr Vec3 LTB{-0.5f, 0.5f, -0.5f};
constexpr Vec3 LTF{-0.5f, 0.5f, 0.5f};
constexpr Vec3 RBB{0.5f, -0.5f, -0.5f};
constexpr Vec3 RBF{0.5f, -0.5f, 0.5f};
constexpr Vec3 RTB{0.5f, 0.5f, -0.5f};
constexpr Vec3 RTF{0.5f, 0.5f, 0.5f};

#define CUBE_SIDE(V1, V2, V3, V4, V5, V6, COLOR)                               \
  {V1, COLOR}, {V2, COLOR}, {V3, COLOR}, {V4, COLOR}, {V5, COLOR}, {V6, COLOR},

constexpr Vertex c_cubeVertices[] = {
    CUBE_SIDE(LTB, LBF, LBB, LTB, LTF, LBF, DarkRed)   // -X
    CUBE_SIDE(RTB, RBB, RBF, RTB, RBF, RTF, Red)       // +X
    CUBE_SIDE(LBB, LBF, RBF, LBB, RBF, RBB, DarkGreen) // -Y
    CUBE_SIDE(LTB, RTB, RTF, LTB, RTF, LTF, Green)     // +Y
    CUBE_SIDE(LBB, RBB, RTB, LBB, RTB, LTB, DarkBlue)  // -Z
    CUBE_SIDE(LBF, LTF, RTF, LBF, RTF, RBF, Blue)      // +Z
};

// Winding order is clockwise. Each side uses a different color.
constexpr unsigned short c_cubeIndices[] = {
    0,  1,  2,  3,  4,  5,  // -X
    6,  7,  8,  9,  10, 11, // +X
    12, 13, 14, 15, 16, 17, // -Y
    18, 19, 20, 21, 22, 23, // +Y
    24, 25, 26, 27, 28, 29, // -Z
    30, 31, 32, 33, 34, 35, // +Z
};

struct Cube {
  XrPosef pose;
  XrVector3f scale;
};

struct ViewRenderer {
  VkDevice device;
  VkQueue queue;
  vko::Pipeline pipeline;
  std::shared_ptr<vko::DepthImage> depthBuffer;
  std::map<VkImage, std::shared_ptr<vko::SwapchainFramebuffer>> framebufferMap;
  vko::Fence execFence;
  VkCommandPool commandPool = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

  ViewRenderer(VkPhysicalDevice physicalDevice, VkDevice _device,
               uint32_t queueFamilyIndex, VkExtent2D extent,
               VkFormat colorFormat, VkFormat depthFormat,
               VkSampleCountFlagBits sampleCountFlagBits,
               vko::Pipeline _pipeline)
      : device(_device), execFence(_device, true),
        pipeline(std::move(_pipeline)) {
    vkGetDeviceQueue(this->device, queueFamilyIndex, 0, &this->queue);

    this->depthBuffer = std::make_shared<vko::DepthImage>(
        this->device, physicalDevice, extent, depthFormat, sampleCountFlagBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkCommandPoolCreateInfo cmdPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };
    vko::VKO_CHECK(vkCreateCommandPool(this->device, &cmdPoolInfo, nullptr,
                                       &this->commandPool));
    if (vko::SetDebugUtilsObjectNameEXT(
            device, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)this->commandPool,
            "hello_xr command pool") != VK_SUCCESS) {
      throw std::runtime_error("SetDebugUtilsObjectNameEXT");
    }

    VkCommandBufferAllocateInfo cmd{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = this->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vko::VKO_CHECK(
        vkAllocateCommandBuffers(this->device, &cmd, &this->commandBuffer));
    if (vko::SetDebugUtilsObjectNameEXT(device, VK_OBJECT_TYPE_COMMAND_BUFFER,
                                        (uint64_t)this->commandBuffer,
                                        "hello_xr command buffer") !=
        VK_SUCCESS) {
      throw std::runtime_error("SetDebugUtilsObjectNameEXT");
    }
  }

  ~ViewRenderer() {
    vkFreeCommandBuffers(this->device, this->commandPool, 1,
                         &this->commandBuffer);
    vkDestroyCommandPool(this->device, this->commandPool, nullptr);
  }

  void render(VkImage image, VkExtent2D size, VkFormat colorFormat,
              VkFormat depthFormat, const VkClearColorValue &clearColor,
              VkBuffer vertices, VkBuffer indices, uint32_t drawCount,
              const XrPosef &hmdPose, XrFovf fov,
              const std::vector<Cube> &cubes) {
    // Waiting on a not-in-flight command buffer is a no-op
    execFence.wait();
    execFence.reset();

    vko::VKO_CHECK(vkResetCommandBuffer(this->commandBuffer, 0));

    VkCommandBufferBeginInfo cmdBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    vko::VKO_CHECK(vkBeginCommandBuffer(this->commandBuffer, &cmdBeginInfo));

    // Ensure depth is in the right layout
    this->depthBuffer->TransitionLayout(
        this->commandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    // Bind and clear eye render target
    static std::array<VkClearValue, 2> clearValues;
    clearValues[0].color = clearColor;
    clearValues[1].depthStencil.depth = 1.0f;
    clearValues[1].depthStencil.stencil = 0;
    VkRenderPassBeginInfo renderPassBeginInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };

    auto found = this->framebufferMap.find(image);
    if (found == this->framebufferMap.end()) {
      auto rt = std::make_shared<vko::SwapchainFramebuffer>(
          this->device, image, size, colorFormat, this->pipeline.renderPass,
          this->depthBuffer->image, depthFormat);
      found = this->framebufferMap.insert({image, rt}).first;
    }
    renderPassBeginInfo.renderPass = this->pipeline.renderPass;
    renderPassBeginInfo.framebuffer = found->second->framebuffer;
    renderPassBeginInfo.renderArea.offset = {0, 0};
    renderPassBeginInfo.renderArea.extent = size;

    vkCmdBeginRenderPass(this->commandBuffer, &renderPassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

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

    vkCmdEndRenderPass(this->commandBuffer);
    vko::VKO_CHECK(vkEndCommandBuffer(this->commandBuffer));

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &this->commandBuffer,
    };
    vko::VKO_CHECK(vkQueueSubmit(this->queue, 1, &submitInfo, execFence));
  }
};
// struct ViewSwapchainInfo {
//   VkImage Image;
//   XrCompositionLayerProjectionView CompositionLayer;
//
//   // XrMatrix4x4f calcViewProjection() const {
//   //   // Compute the view-projection transform. Note all matrixes
//   //   // (including OpenXR's) are column-major, right-handed.
//   //   XrMatrix4x4f proj;
//   //   XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_VULKAN,
//   //                                    this->CompositionLayer.fov, 0.05f,
//   100.0f);
//   //   XrMatrix4x4f toView;
//   //   XrMatrix4x4f_CreateFromRigidTransform(&toView,
//   //                                         &this->CompositionLayer.pose);
//   //   XrMatrix4x4f view;
//   //   XrMatrix4x4f_InvertRigidBody(&view, &toView);
//   //   XrMatrix4x4f vp;
//   //   XrMatrix4x4f_Multiply(&vp, &proj, &view);
//   //
//   //   return vp;
//   // }
// };

struct VisualizedSpaces : public vko::not_copyable {
  std::vector<XrSpace> m_visualizedSpaces;

  VisualizedSpaces(XrSession session) {
    std::string visualizedSpaces[] = {
        "ViewFront",        "Local",      "Stage",
        "StageLeft",        "StageRight", "StageLeftRotated",
        "StageRightRotated"};

    for (const auto &visualizedSpace : visualizedSpaces) {
      auto referenceSpaceCreateInfo =
          GetXrReferenceSpaceCreateInfo(visualizedSpace);
      XrSpace space;
      XrResult res =
          xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &space);
      if (XR_SUCCEEDED(res)) {
        m_visualizedSpaces.push_back(space);
      } else {
        xro::Logger::Error("Failed to create reference space %s with error %d",
                           visualizedSpace.c_str(), res);
      }
    }
  }

  ~VisualizedSpaces() {
    for (XrSpace visualizedSpace : m_visualizedSpaces) {
      xrDestroySpace(visualizedSpace);
    }
  }
};

void xr_vulkan_session(const std::function<bool(bool)> &runLoop,
                       XrInstance instance, XrSystemId systemId,
                       XrSession session, XrSpace appSpace,
                       XrEnvironmentBlendMode blendMode,
                       VkClearColorValue clearColor,
                       XrViewConfigurationType viewConfigurationType,
                       VkFormat viewFormat, VkInstance vkInstance,
                       VkPhysicalDevice physicalDevice,
                       uint32_t queueFamilyIndex, VkDevice device) {
  // debug
  vko::g_vkSetDebugUtilsObjectNameEXT(vkInstance);

  static_assert(sizeof(Vertex) == 24, "Unexpected Vertex size");
  vko::IndexedMesh mesh = {
      .inputBindingDescriptions =
          {
              {
                  .binding = 0,
                  .stride = static_cast<uint32_t>(sizeof(Vertex)),
                  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
              },
          },
      .attributeDescriptions =
          {
              {
                  .location = 0,
                  .binding = 0,
                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                  .offset = offsetof(Vertex, Position),
              },
              {
                  .location = 1,
                  .binding = 0,
                  .format = VK_FORMAT_R32G32B32_SFLOAT,
                  .offset = offsetof(Vertex, Color),
              },
          },
  };
  {
    VkDeviceSize bufferSize = sizeof(c_cubeVertices);
    mesh.vertexBuffer = std::make_shared<vko::Buffer>(
        physicalDevice, device, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vko::copyBytesToBufferCommand(physicalDevice, device, queueFamilyIndex,
                                  c_cubeVertices, bufferSize,
                                  mesh.vertexBuffer->buffer);
  }
  {
    mesh.indexDrawCount = std::size(c_cubeIndices);
    VkDeviceSize bufferSize = sizeof(c_cubeIndices);
    mesh.indexBuffer = std::make_shared<vko::Buffer>(
        physicalDevice, device, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vko::copyBytesToBufferCommand(physicalDevice, device, queueFamilyIndex,
                                  c_cubeIndices, bufferSize,
                                  mesh.indexBuffer->buffer);
  }

  // Create resources for each view.
  std::vector<std::shared_ptr<xro::Swapchain>> swapchains;
  std::vector<std::shared_ptr<ViewRenderer>> renderers;

  auto depthFormat = VK_FORMAT_D32_SFLOAT;

  xro::SessionState state(instance, session, viewConfigurationType);
  VisualizedSpaces spaces(session);
  xro::InputState input(instance, session);

  xro::Stereoscope stereoscope(instance, systemId, viewConfigurationType);

  for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
    // XrSwapchain
    auto swapchain = std::make_shared<xro::Swapchain>(
        session, i, stereoscope.viewConfigurations[i], viewFormat);
    swapchains.push_back(swapchain);

    // pieline
    auto pipelineLayout =
        vko::createPipelineLayoutWithConstantSize(device, sizeof(float) * 16);
    auto renderPass =
        vko::createColorDepthRenderPass(device, viewFormat, depthFormat);

    auto vertexSPIRV = vko::glsl_vs_to_spv(VertexShaderGlsl);
    assert(vertexSPIRV.size());
    auto vs =
        vko::ShaderModule::createVertexShader(device, vertexSPIRV, "main");

    auto fragmentSPIRV = vko::glsl_fs_to_spv(FragmentShaderGlsl);
    assert(fragmentSPIRV.size());
    auto fs =
        vko::ShaderModule::createFragmentShader(device, fragmentSPIRV, "main");

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        vs.pipelineShaderStageCreateInfo,
        fs.pipelineShaderStageCreateInfo,
    };

    auto extent = swapchain->extent();
    VkRect2D scissor = {{0, 0}, extent};
    // Will invert y after projection
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    vko::PipelineBuilder builder;
    auto pipeline =
        builder.create(device, renderPass, pipelineLayout, stages,
                       mesh.inputBindingDescriptions,
                       mesh.attributeDescriptions, {viewport}, {scissor}, {});

    auto ptr = std::make_shared<ViewRenderer>(
        physicalDevice, device, queueFamilyIndex, swapchain->extent(),
        swapchain->format(), depthFormat, swapchain->sampleCountFlagBits(),
        std::move(pipeline));
    renderers.push_back(ptr);
  }

  // mainloop
  bool isSessionRunning = true;
  while (runLoop(isSessionRunning)) {
    auto poll = state.PollEvents();
    if (poll.exitRenderLoop) {
      break;
    }

    if (!poll.isSessionRunning) {
      // Throttle loop since xrWaitFrame won't be called.
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
      continue;
    }

    input.PollActions();

    auto frameState = xro::beginFrame(session);
    xro::LayerComposition composition(blendMode, appSpace);

    if (frameState.shouldRender == XR_TRUE) {
      if (stereoscope.Locate(session, appSpace, frameState.predictedDisplayTime,
                             viewConfigurationType)) {
        // XrCompositionLayerProjection

        // update scene
        std::vector<Cube> cubes;

        // For each locatable space that we want to visualize, render a 25cm
        for (XrSpace visualizedSpace : spaces.m_visualizedSpaces) {
          auto [res, location] = xro::locate(
              appSpace, frameState.predictedDisplayTime, visualizedSpace);
          if (xro::locationIsValid(res, location)) {
            cubes.push_back({location.pose, {0.25f, 0.25f, 0.25f}});
          }
        }
        for (auto &hand : input.hands) {
          auto [res, location] = xro::locate(
              appSpace, frameState.predictedDisplayTime, hand.space);
          auto s = hand.scale * 0.1f;
          if (xro::locationIsValid(res, location)) {
            cubes.push_back({location.pose, {s, s, s}});
          }
        }

        for (uint32_t i = 0; i < stereoscope.views.size(); ++i) {
          // XrCompositionLayerProjectionView(left / right)
          auto swapchain = swapchains[i];
          auto [image, projectionLayer] =
              swapchain->AcquireSwapchain(stereoscope.views[i]);
          composition.pushView(projectionLayer);

          renderers[i]->render(
              image, swapchain->extent(), swapchain->format(), depthFormat,
              clearColor, mesh.vertexBuffer->buffer, mesh.indexBuffer->buffer,
              mesh.indexDrawCount, projectionLayer.pose, projectionLayer.fov,
              cubes);

          swapchain->EndSwapchain();
        }
      }
    }

    // std::vector<XrCompositionLayerBaseHeader *>
    auto &layers = composition.commitLayers();
    xro::endFrame(session, frameState.predictedDisplayTime, blendMode, layers);
  }

  vkDeviceWaitIdle(device);
}

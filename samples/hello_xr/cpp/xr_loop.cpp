#include "xr_loop.h"
#include "openxr_program/CubeScene.h"
#include "openxr_program/openxr_swapchain.h"
#include "openxr_program/options.h"
#include <map>
#include <vko/vko_pipeline.h>
#include <vko/vko_shaderc.h>

char VertexShaderGlsl[] = {
#embed "shader.vert"
    , 0};

char FragmentShaderGlsl[] = {
#embed "shader.frag"
    , 0};

static_assert(sizeof(VertexShaderGlsl), "VertexShaderGlsl");
static_assert(sizeof(FragmentShaderGlsl), "FragmentShaderGlsl");

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

struct ViewRenderer {
  VkDevice device;
  VkQueue queue;
  vko::Fence execFence;
  VkCommandPool commandPool = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

  VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  std::vector<VkDynamicState> dynamicStateEnables;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;

  std::shared_ptr<vko::DepthImage> depthBuffer;
  std::map<VkImage, std::shared_ptr<vko::SwapchainFramebuffer>> framebufferMap;

  ViewRenderer(VkPhysicalDevice physicalDevice, VkDevice _device,
               uint32_t queueFamilyIndex, VkExtent2D extent,
               VkFormat colorFormat, VkFormat depthFormat,
               VkSampleCountFlagBits sampleCountFlagBits,
               const std::vector<VkVertexInputBindingDescription>
                   &vertexInputBindingDescription,
               const std::vector<VkVertexInputAttributeDescription>
                   &vertexInputAttributeDescription)
      : device(_device), execFence(_device, true) {
    vkGetDeviceQueue(this->device, queueFamilyIndex, 0, &this->queue);

    this->depthBuffer = std::make_shared<vko::DepthImage>(
        this->device, physicalDevice, extent, depthFormat, sampleCountFlagBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    this->pipelineLayout =
        vko::createPipelineLayoutWithConstantSize(device, sizeof(float) * 16);
    this->renderPass =
        vko::createColorDepthRenderPass(device, colorFormat, depthFormat);
    VkPipelineDynamicStateCreateInfo dynamicState{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = (uint32_t)this->dynamicStateEnables.size();
    dynamicState.pDynamicStates = this->dynamicStateEnables.data();

    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount =
        static_cast<uint32_t>(std::size(vertexInputBindingDescription));
    vi.pVertexBindingDescriptions = vertexInputBindingDescription.data();
    vi.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(std::size(vertexInputAttributeDescription));
    vi.pVertexAttributeDescriptions = vertexInputAttributeDescription.data();

    VkPipelineInputAssemblyStateCreateInfo ia{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.primitiveRestartEnable = VK_FALSE;
    ia.topology = this->topology;

    VkPipelineRasterizationStateCreateInfo rs{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.depthClampEnable = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.depthBiasEnable = VK_FALSE;
    rs.depthBiasConstantFactor = 0;
    rs.depthBiasClamp = 0;
    rs.depthBiasSlopeFactor = 0;
    rs.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState attachState{};
    attachState.blendEnable = 0;
    attachState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    attachState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    attachState.colorBlendOp = VK_BLEND_OP_ADD;
    attachState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attachState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    attachState.alphaBlendOp = VK_BLEND_OP_ADD;
    attachState.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &attachState;
    cb.logicOpEnable = VK_FALSE;
    cb.logicOp = VK_LOGIC_OP_NO_OP;
    cb.blendConstants[0] = 1.0f;
    cb.blendConstants[1] = 1.0f;
    cb.blendConstants[2] = 1.0f;
    cb.blendConstants[3] = 1.0f;

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
    VkPipelineViewportStateCreateInfo vp{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.pViewports = &viewport;
    vp.scissorCount = 1;
    vp.pScissors = &scissor;

    VkPipelineDepthStencilStateCreateInfo ds{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable = VK_FALSE;
    ds.front.failOp = VK_STENCIL_OP_KEEP;
    ds.front.passOp = VK_STENCIL_OP_KEEP;
    ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
    ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
    ds.back = ds.front;
    ds.minDepthBounds = 0.0f;
    ds.maxDepthBounds = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    auto vertexSPIRV = vko::glsl_vs_to_spv(VertexShaderGlsl);
    assert(vertexSPIRV.size());
    auto vs =
        vko::ShaderModule::createVertexShader(device, vertexSPIRV, "main");

    auto fragmentSPIRV = vko::glsl_fs_to_spv(FragmentShaderGlsl);
    assert(fragmentSPIRV.size());
    auto fs =
        vko::ShaderModule::createFragmentShader(device, fragmentSPIRV, "main");

    VkPipelineShaderStageCreateInfo stages[] = {
        vs.pipelineShaderStageCreateInfo,
        fs.pipelineShaderStageCreateInfo,
    };

    VkGraphicsPipelineCreateInfo pipeInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = static_cast<uint32_t>(std::size(stages)),
        .pStages = stages,
        .pVertexInputState = &vi,
        .pInputAssemblyState = &ia,
        .pTessellationState = nullptr,
        .pViewportState = &vp,
        .pRasterizationState = &rs,
        .pMultisampleState = &ms,
        .pDepthStencilState = &ds,
        .pColorBlendState = &cb,
        .layout = this->pipelineLayout,
        .renderPass = this->renderPass,
        .subpass = 0,
    };
    if (dynamicState.dynamicStateCount > 0) {
      pipeInfo.pDynamicState = &dynamicState;
    }
    if (vkCreateGraphicsPipelines(this->device, VK_NULL_HANDLE, 1, &pipeInfo,
                                  nullptr, &this->pipeline) != VK_SUCCESS) {
      throw std::runtime_error("vkCreateGraphicsPipelines");
    }

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

    if (this->pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(this->device, this->pipeline, nullptr);
    }
    if (this->pipelineLayout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(this->device, this->pipelineLayout, nullptr);
    }
    if (this->renderPass != VK_NULL_HANDLE) {
      vkDestroyRenderPass(this->device, this->renderPass, nullptr);
    }
  }

  void render(VkImage image, VkExtent2D size, VkFormat colorFormat,
              VkFormat depthFormat, const Vec4 &clearColor,
              const std::vector<Mat4> &matrices, VkBuffer vertices,
              VkBuffer indices, uint32_t drawCount) {
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

    auto found = this->framebufferMap.find(image);
    if (found == this->framebufferMap.end()) {
      auto rt = std::make_shared<vko::SwapchainFramebuffer>(
          this->device, image, size, colorFormat, this->renderPass,
          this->depthBuffer->image, depthFormat);
      found = this->framebufferMap.insert({image, rt}).first;
    }
    renderPassBeginInfo.renderPass = this->renderPass;
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
    for (const Mat4 &mat : matrices) {
      vkCmdPushConstants(this->commandBuffer, this->pipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat), &mat.m[0]);

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

void xr_loop(const std::function<bool()> &runLoop, const Options &options,
             OpenXrSession &session, VkPhysicalDevice physicalDevice,
             uint32_t queueFamilyIndex, VkDevice device) {

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
  auto config = session.GetSwapchainConfiguration();
  std::vector<std::shared_ptr<OpenXrSwapchain>> swapchains;
  std::vector<std::shared_ptr<ViewRenderer>> views;

  auto depthFormat = VK_FORMAT_D32_SFLOAT;

  for (uint32_t i = 0; i < config.Views.size(); i++) {
    // XrSwapchain
    auto swapchain = OpenXrSwapchain::Create(session.m_session, i,
                                             config.Views[i], config.Format);
    swapchains.push_back(swapchain);

    auto ptr = std::make_shared<ViewRenderer>(
        physicalDevice, device, queueFamilyIndex, swapchain->extent(),
        swapchain->format(), depthFormat, swapchain->sampleCountFlagBits(),
        mesh.inputBindingDescriptions, mesh.attributeDescriptions);
    views.push_back(ptr);
  }

  // mainloop
  while (runLoop()) {

    auto frameState = session.BeginFrame();
    LayerComposition composition(options.Parsed.EnvironmentBlendMode,
                                 session.m_appSpace);

    if (frameState.shouldRender == XR_TRUE) {
      uint32_t viewCountOutput;
      if (session.LocateView(session.m_appSpace,
                             frameState.predictedDisplayTime,
                             options.Parsed.ViewConfigType, &viewCountOutput)) {
        // XrCompositionLayerProjection

        // update scene
        CubeScene scene;
        scene.addSpaceCubes(session.m_appSpace, frameState.predictedDisplayTime,
                            session.m_visualizedSpaces);
        scene.addHandCubes(session.m_appSpace, frameState.predictedDisplayTime,
                           session.m_input);

        for (uint32_t i = 0; i < viewCountOutput; ++i) {

          // XrCompositionLayerProjectionView(left / right)
          auto swapchain = swapchains[i];
          auto info = swapchain->AcquireSwapchain(session.m_views[i]);
          composition.pushView(info.CompositionLayer);

          views[i]->render(info.Image, swapchain->extent(), swapchain->format(),
                           depthFormat, options.GetBackgroundClearColor(),
                           scene.CalcCubeMatrices(info.calcViewProjection()),
                           mesh.vertexBuffer->buffer, mesh.indexBuffer->buffer,
                           mesh.indexDrawCount);

          swapchain->EndSwapchain();
        }
      }
    }

    // std::vector<XrCompositionLayerBaseHeader *>
    auto &layers = composition.commitLayers();
    session.EndFrame(frameState.predictedDisplayTime, layers);
  }

  vkDeviceWaitIdle(device);
}

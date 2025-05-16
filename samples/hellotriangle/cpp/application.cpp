#include "application.hpp"
#include "common.hpp"
#include "math.hpp"
#include "platform.hpp"
#include <string.h>

static std::vector<uint8_t> readRawFile(AAssetManager *assetManager,
                                        const char *pPath) {
  if (!assetManager) {
    LOGE("Asset manager does not exist.");
    return {};
  }

  AAsset *asset = AAssetManager_open(assetManager, pPath, AASSET_MODE_BUFFER);
  if (!asset) {
    LOGE("AAssetManager_open() failed to load file: %s.", pPath);
    return {};
  }

  auto buffer = AAsset_getBuffer(asset);
  if (!buffer) {
    LOGE("Failed to obtain buffer for asset: %s.", pPath);
    AAsset_close(asset);
    return {};
  }

  auto len = AAsset_getLength(asset);
  std::vector<uint8_t> out(len);

  memcpy(out.data(), buffer, len);
  AAsset_close(asset);
  return out;
}

template <typename T>
inline std::vector<T> readBinaryFile(AAssetManager *assetManager,
                                     const char *pPath) {
  auto raw = readRawFile(assetManager, pPath);
  if (raw.empty()) {
    return {};
  }

  size_t numElements = raw.size() / sizeof(T);
  std::vector<T> buffer(numElements);
  memcpy(buffer.data(), raw.data(), raw.size());
  return buffer;
}

static VkShaderModule loadShaderModule(VkDevice device,
                                       const std::vector<uint32_t> &buffer) {
  if (buffer.empty()) {
    return VK_NULL_HANDLE;
  }

  VkShaderModuleCreateInfo moduleInfo = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  moduleInfo.codeSize = buffer.size() * sizeof(uint32_t);
  moduleInfo.pCode = buffer.data();

  VkShaderModule shaderModule;
  VK_CHECK(vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule));
  return shaderModule;
}

struct Vertex {
  glm::vec4 position;
  glm::vec4 color;
};

//
// VulkanApplication
//
VulkanApplication::VulkanApplication(VkDevice device,
                                     MaliSDK::Platform *platform)
    : _device(device), pContext(platform) {
  // Create the vertex buffer.
  initVertexBuffer(pContext->getMemoryProperties());

  // Create a pipeline cache (although we'll only create one pipeline).
  VkPipelineCacheCreateInfo pipelineCacheInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
  VK_CHECK(vkCreatePipelineCache(_device, &pipelineCacheInfo, nullptr,
                                 &pipelineCache));
}

std::shared_ptr<VulkanApplication>
VulkanApplication::create(MaliSDK::Platform *platform,
                          AAssetManager *assetManager) {
  auto ptr = std::shared_ptr<VulkanApplication>(
      new VulkanApplication(platform->getDevice(), platform));

  LOGI("Updating swapchain!\n");
  ptr->updateSwapchain(assetManager, platform->swapchainImages,
                       platform->swapchainDimensions);
  return ptr;
}

// To create a buffer, both the device and application have requirements from
// the buffer object.
// Vulkan exposes the different types of buffers the device can allocate, and we
// have to find a suitable one.
// deviceRequirements is a bitmask expressing which memory types can be used for
// a buffer object.
// The different memory types' properties must match with what the application
// wants.
static uint32_t
findMemoryTypeFromRequirements(const VkPhysicalDeviceMemoryProperties &props,
                               uint32_t deviceRequirements,
                               uint32_t hostRequirements) {
  for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
    if (deviceRequirements & (1u << i)) {
      if ((props.memoryTypes[i].propertyFlags & hostRequirements) ==
          hostRequirements) {
        return i;
      }
    }
  }

  LOGE("Failed to obtain suitable memory type.\n");
  abort();
}

Buffer
VulkanApplication::createBuffer(const VkPhysicalDeviceMemoryProperties &props,
                                const void *pInitialData, size_t size,
                                VkFlags usage) {

  VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  info.usage = usage;
  info.size = size;

  Buffer buffer;
  VK_CHECK(vkCreateBuffer(_device, &info, nullptr, &buffer.buffer));

  // Ask device about its memory requirements.
  VkMemoryRequirements memReqs;
  vkGetBufferMemoryRequirements(_device, buffer.buffer, &memReqs);

  VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  alloc.allocationSize = memReqs.size;

  // We want host visible and coherent memory to simplify things.
  alloc.memoryTypeIndex =
      findMemoryTypeFromRequirements(props, memReqs.memoryTypeBits,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Allocate memory.
  VK_CHECK(vkAllocateMemory(_device, &alloc, nullptr, &buffer.memory));

  // Buffers are not backed by memory, so bind our memory explicitly to the
  // buffer.
  vkBindBufferMemory(_device, buffer.buffer, buffer.memory, 0);

  // Map the memory and dump data in there.
  if (pInitialData) {
    void *pData;
    VK_CHECK(vkMapMemory(_device, buffer.memory, 0, size, 0, &pData));
    memcpy(pData, pInitialData, size);
    vkUnmapMemory(_device, buffer.memory);
  }

  return buffer;
}

void VulkanApplication::initRenderPass(VkFormat format) {
  VkAttachmentDescription attachment = {0};
  // Backbuffer format.
  attachment.format = format;
  // Not multisampled.
  attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  // When starting the frame, we want tiles to be cleared.
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  // When ending the frame, we want tiles to be written out.
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  // Don't care about stencil since we're not using it.
  attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  // The image layout will be undefined when the render pass begins.
  attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  // After the render pass is complete, we will transition to PRESENT_SRC_KHR
  // layout.
  attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  // We have one subpass.
  // This subpass has 1 color attachment.
  // While executing this subpass, the attachment will be in attachment optimal
  // layout.
  VkAttachmentReference colorRef = {0,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  // We will end up with two transitions.
  // The first one happens right before we start subpass #0, where
  // UNDEFINED is transitioned into COLOR_ATTACHMENT_OPTIMAL.
  // The final layout in the render pass attachment states PRESENT_SRC_KHR, so
  // we
  // will get a final transition from COLOR_ATTACHMENT_OPTIMAL to
  // PRESENT_SRC_KHR.

  VkSubpassDescription subpass = {0};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;

  // Create a dependency to external events.
  // We need to wait for the WSI semaphore to signal.
  // Only pipeline stages which depend on COLOR_ATTACHMENT_OUTPUT_BIT will
  // actually wait for the semaphore, so we must also wait for that pipeline
  // stage.
  VkSubpassDependency dependency = {0};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  // Since we changed the image layout, we need to make the memory visible to
  // color attachment to modify.
  dependency.srcAccessMask = 0;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  // Finally, create the renderpass.
  VkRenderPassCreateInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  rpInfo.attachmentCount = 1;
  rpInfo.pAttachments = &attachment;
  rpInfo.subpassCount = 1;
  rpInfo.pSubpasses = &subpass;
  rpInfo.dependencyCount = 1;
  rpInfo.pDependencies = &dependency;

  VK_CHECK(vkCreateRenderPass(_device, &rpInfo, nullptr, &renderPass));
}

void VulkanApplication::initVertexBuffer(
    const VkPhysicalDeviceMemoryProperties &props) {
  // A simple counter-clockwise triangle.
  // We specify the positions directly in clip space.
  static const Vertex data[] = {
      {
          glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f),
          glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
      },
      {
          glm::vec4(-0.5f, +0.5f, 0.0f, 1.0f),
          glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
      },
      {
          glm::vec4(+0.5f, -0.5f, 0.0f, 1.0f),
          glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
      },
  };

  // We will use the buffer as a vertex buffer only.
  vertexBuffer = createBuffer(props, data, sizeof(data),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

void VulkanApplication::initPipeline(AAssetManager *assetManager) {
  // Create a blank pipeline layout.
  // We are not binding any resources to the pipeline in this first sample.
  VkPipelineLayoutCreateInfo layoutInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  VK_CHECK(
      vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &pipelineLayout));

  // Specify we will use triangle lists to draw geometry.
  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  // Specify our two attributes, Position and Color.
  VkVertexInputAttributeDescription attributes[2] = {{0}};
  attributes[0].location = 0; // Position in shader specifies layout(location =
  // 0) to link with this attribute.
  attributes[0].binding = 0; // Uses vertex buffer #0.
  attributes[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
  attributes[0].offset = 0;
  attributes[1].location = 1; // Color in shader specifies layout(location = 1)
  // to link with this attribute.
  attributes[1].binding = 0; // Uses vertex buffer #0.
  attributes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
  attributes[1].offset = 4 * sizeof(float);

  // We have one vertex buffer, with stride 8 floats (vec4 + vec4).
  VkVertexInputBindingDescription binding = {0};
  binding.binding = 0;
  binding.stride =
      sizeof(Vertex); // We specify the buffer stride up front here.
  // The vertex buffer will step for every vertex (rather than per instance).
  binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkPipelineVertexInputStateCreateInfo vertexInput = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vertexInput.vertexBindingDescriptionCount = 1;
  vertexInput.pVertexBindingDescriptions = &binding;
  vertexInput.vertexAttributeDescriptionCount = 2;
  vertexInput.pVertexAttributeDescriptions = attributes;

  // Specify rasterization state.
  VkPipelineRasterizationStateCreateInfo raster = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.cullMode = VK_CULL_MODE_BACK_BIT;
  raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  raster.depthClampEnable = false;
  raster.rasterizerDiscardEnable = false;
  raster.depthBiasEnable = false;
  raster.lineWidth = 1.0f;

  // Our attachment will write to all color channels, but no blending is
  // enabled.
  VkPipelineColorBlendAttachmentState blendAttachment = {0};
  blendAttachment.blendEnable = false;
  blendAttachment.colorWriteMask = 0xf;

  VkPipelineColorBlendStateCreateInfo blend = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  blend.attachmentCount = 1;
  blend.pAttachments = &blendAttachment;

  // We will have one viewport and scissor box.
  VkPipelineViewportStateCreateInfo viewport = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewport.viewportCount = 1;
  viewport.scissorCount = 1;

  // Disable all depth testing.
  VkPipelineDepthStencilStateCreateInfo depthStencil = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  depthStencil.depthTestEnable = false;
  depthStencil.depthWriteEnable = false;
  depthStencil.depthBoundsTestEnable = false;
  depthStencil.stencilTestEnable = false;

  // No multisampling.
  VkPipelineMultisampleStateCreateInfo multisample = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // Specify that these states will be dynamic, i.e. not part of pipeline state
  // object.
  static const VkDynamicState dynamics[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };
  VkPipelineDynamicStateCreateInfo dynamic = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamic.pDynamicStates = dynamics;
  dynamic.dynamicStateCount = sizeof(dynamics) / sizeof(dynamics[0]);

  // Load our SPIR-V shaders.
  VkPipelineShaderStageCreateInfo shaderStages[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
  };

  // We have two pipeline stages, vertex and fragment.
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = loadShaderModule(
      _device,
      readBinaryFile<uint32_t>(assetManager, "shaders/triangle.vert.spv"));
  shaderStages[0].pName = "main";
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = loadShaderModule(
      _device,
      readBinaryFile<uint32_t>(assetManager, "shaders/triangle.frag.spv"));
  shaderStages[1].pName = "main";

  VkGraphicsPipelineCreateInfo pipe = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pipe.stageCount = 2;
  pipe.pStages = shaderStages;
  pipe.pVertexInputState = &vertexInput;
  pipe.pInputAssemblyState = &inputAssembly;
  pipe.pRasterizationState = &raster;
  pipe.pColorBlendState = &blend;
  pipe.pMultisampleState = &multisample;
  pipe.pViewportState = &viewport;
  pipe.pDepthStencilState = &depthStencil;
  pipe.pDynamicState = &dynamic;

  // We need to specify the pipeline layout and the render pass description up
  // front as well.
  pipe.renderPass = renderPass;
  pipe.layout = pipelineLayout;

  VK_CHECK(vkCreateGraphicsPipelines(_device, pipelineCache, 1, &pipe, nullptr,
                                     &pipeline));

  // Pipeline is baked, we can delete the shader modules now.
  vkDestroyShaderModule(_device, shaderStages[0].module, nullptr);
  vkDestroyShaderModule(_device, shaderStages[1].module, nullptr);
}

void VulkanApplication::render(unsigned swapchainIndex, float /*deltaTime*/) {
  // Render to this backbuffer.
  Backbuffer &backbuffer = backbuffers[swapchainIndex];

  // Request a fresh command buffer.
  VkCommandBuffer cmd = pContext->requestPrimaryCommandBuffer();

  // We will only submit this once before it's recycled.
  VkCommandBufferBeginInfo beginInfo = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &beginInfo);

  // Set clear color values.
  VkClearValue clearValue;
  clearValue.color.float32[0] = 0.1f;
  clearValue.color.float32[1] = 0.1f;
  clearValue.color.float32[2] = 0.2f;
  clearValue.color.float32[3] = 1.0f;

  // Begin the render pass.
  VkRenderPassBeginInfo rpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  rpBegin.renderPass = renderPass;
  rpBegin.framebuffer = backbuffer.framebuffer;
  rpBegin.renderArea.extent.width = width;
  rpBegin.renderArea.extent.height = height;
  rpBegin.clearValueCount = 1;
  rpBegin.pClearValues = &clearValue;
  // We will add draw commands in the same command buffer.
  vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

  // Bind the graphics pipeline.
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  // Set up dynamic state.
  // Viewport
  VkViewport vp = {0};
  vp.x = 0.0f;
  vp.y = 0.0f;
  vp.width = float(width);
  vp.height = float(height);
  vp.minDepth = 0.0f;
  vp.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &vp);

  // Scissor box
  VkRect2D scissor;
  memset(&scissor, 0, sizeof(scissor));
  scissor.extent.width = width;
  scissor.extent.height = height;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // Bind vertex buffer.
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, &offset);

  // Draw three vertices with one instance.
  vkCmdDraw(cmd, 3, 1, 0, 0);

  // Complete render pass.
  vkCmdEndRenderPass(cmd);

  // Complete the command buffer.
  VK_CHECK(vkEndCommandBuffer(cmd));

  // Submit it to the queue.
  pContext->submitSwapchain(cmd);
}

void VulkanApplication::termBackbuffers() {
  // Tear down backbuffers.
  // If our swapchain changes, we will call this, and create a new swapchain.

  if (!backbuffers.empty()) {
    // Wait until device is idle before teardown.
    vkQueueWaitIdle(pContext->getGraphicsQueue());
    for (auto &backbuffer : backbuffers) {
      vkDestroyFramebuffer(_device, backbuffer.framebuffer, nullptr);
      vkDestroyImageView(_device, backbuffer.view, nullptr);
    }
    backbuffers.clear();
    vkDestroyRenderPass(_device, renderPass, nullptr);
    vkDestroyPipeline(_device, pipeline, nullptr);
    vkDestroyPipelineLayout(_device, pipelineLayout, nullptr);
  }
}

void VulkanApplication::terminate() {
  vkDeviceWaitIdle(_device);

  // Final teardown.
  vkFreeMemory(_device, vertexBuffer.memory, nullptr);
  vkDestroyBuffer(_device, vertexBuffer.buffer, nullptr);

  termBackbuffers();
  vkDestroyPipelineCache(_device, pipelineCache, nullptr);
}

void VulkanApplication::updateSwapchain(
    AAssetManager *assetManager, const std::vector<VkImage> &newBackbuffers,
    const MaliSDK::SwapchainDimensions &dim) {
  width = dim.width;
  height = dim.height;

  // In case we're reinitializing the swapchain, terminate the old one first.
  termBackbuffers();

  // We can't initialize the renderpass until we know the swapchain format.
  initRenderPass(dim.format);
  // We can't initialize the pipeline until we know the render pass.
  initPipeline(assetManager);

  // For all backbuffers in the swapchain ...
  for (auto image : newBackbuffers) {
    Backbuffer backbuffer;
    backbuffer.image = image;

    // Create an image view which we can render into.
    VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = dim.format;
    view.image = image;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.layerCount = 1;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.components.r = VK_COMPONENT_SWIZZLE_R;
    view.components.g = VK_COMPONENT_SWIZZLE_G;
    view.components.b = VK_COMPONENT_SWIZZLE_B;
    view.components.a = VK_COMPONENT_SWIZZLE_A;

    VK_CHECK(vkCreateImageView(_device, &view, nullptr, &backbuffer.view));

    // Build the framebuffer.
    VkFramebufferCreateInfo fbInfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbInfo.renderPass = renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &backbuffer.view;
    fbInfo.width = width;
    fbInfo.height = height;
    fbInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(_device, &fbInfo, nullptr,
                                 &backbuffer.framebuffer));

    backbuffers.push_back(backbuffer);
  }
}

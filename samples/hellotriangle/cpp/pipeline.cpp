#include "pipeline.hpp"
#include "common.hpp"
#include "math.hpp"
#include <vector>

struct Vertex {
  glm::vec4 position;
  glm::vec4 color;
};

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

static Buffer createBuffer(VkDevice device,
                           const VkPhysicalDeviceMemoryProperties &props,
                           const void *pInitialData, size_t size,
                           VkFlags usage) {

  VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  info.usage = usage;
  info.size = size;

  Buffer buffer;
  VK_CHECK(vkCreateBuffer(device, &info, nullptr, &buffer.buffer));

  // Ask device about its memory requirements.
  VkMemoryRequirements memReqs;
  vkGetBufferMemoryRequirements(device, buffer.buffer, &memReqs);

  VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  alloc.allocationSize = memReqs.size;

  // We want host visible and coherent memory to simplify things.
  alloc.memoryTypeIndex =
      findMemoryTypeFromRequirements(props, memReqs.memoryTypeBits,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Allocate memory.
  VK_CHECK(vkAllocateMemory(device, &alloc, nullptr, &buffer.memory));

  // Buffers are not backed by memory, so bind our memory explicitly to the
  // buffer.
  vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0);

  // Map the memory and dump data in there.
  if (pInitialData) {
    void *pData;
    VK_CHECK(vkMapMemory(device, buffer.memory, 0, size, 0, &pData));
    memcpy(pData, pInitialData, size);
    vkUnmapMemory(device, buffer.memory);
  }

  return buffer;
}

Pipeline::Pipeline(VkDevice device, VkRenderPass renderPass,
                   VkPipelineLayout pipelineLayout, VkPipeline pipeline,
                   VkPipelineCache pipelineCache)
    : _device(device), _renderPass(renderPass), _pipelineLayout(pipelineLayout),
      _pipeline(pipeline), _pipelineCache(pipelineCache) {}

Pipeline::~Pipeline() {
  vkFreeMemory(_device, vertexBuffer.memory, nullptr);
  vkDestroyBuffer(_device, vertexBuffer.buffer, nullptr);

  vkDestroyPipelineCache(_device, _pipelineCache, nullptr);
  vkDestroyPipeline(_device, _pipeline, nullptr);
  vkDestroyRenderPass(_device, _renderPass, nullptr);
  vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
}

std::shared_ptr<Pipeline>
Pipeline::create(VkDevice device, VkFormat format, AAssetManager *assetManager,
                 const VkPhysicalDeviceMemoryProperties &props) {
  //
  // RenderPass
  //
  // Finally, create the renderpass.
  VkRenderPass renderPass;
  VkAttachmentDescription attachment = {
      // Backbuffer format.
      .format = format,
      // Not multisampled.
      .samples = VK_SAMPLE_COUNT_1_BIT,
      // When starting the frame, we want tiles to be cleared.
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      // When ending the frame, we want tiles to be written out.
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      // Don't care about stencil since we're not using it.
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

      // The image layout will be undefined when the render pass begins.
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      // After the render pass is complete, we will transition to
      // PRESENT_SRC_KHR
      // layout.
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference colorRef = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorRef,
  };

  VkSubpassDependency dependency = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      // Since we changed the image layout, we need to make the memory visible
      // to color attachment to modify.
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };

  VkRenderPassCreateInfo rpInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &attachment,
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 1,
      .pDependencies = &dependency,
  };
  VK_CHECK(vkCreateRenderPass(device, &rpInfo, nullptr, &renderPass));

  //
  // Pipeline
  //
  VkPipelineCache pipelineCache;
  VkPipelineCacheCreateInfo pipelineCacheInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
  };
  VK_CHECK(vkCreatePipelineCache(device, &pipelineCacheInfo, nullptr,
                                 &pipelineCache));

  // Load our SPIR-V shaders.
  VkPipelineShaderStageCreateInfo shaderStages[2] = {
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          // We have two pipeline stages, vertex and fragment.
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = loadShaderModule(
              device, readBinaryFile<uint32_t>(assetManager,
                                               "shaders/triangle.vert.spv")),
          .pName = "main",
      },
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = loadShaderModule(
              device, readBinaryFile<uint32_t>(assetManager,
                                               "shaders/triangle.frag.spv")),
          .pName = "main",
      },
  };

  VkPipeline pipeline;
  // We have one vertex buffer, with stride 8 floats (vec4 + vec4).
  VkVertexInputBindingDescription binding = {
      .binding = 0,
      .stride = sizeof(Vertex), // We specify the buffer stride up front here.
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };

  // Specify our two attributes, Position and Color.
  VkVertexInputAttributeDescription attributes[2] = {
      {
          .location = 0, // Position in shader specifies layout(location = 0) to
                         // link with this attribute.
          .binding = 0,  // Uses vertex buffer #0.
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = 0,
      },
      {
          .location = 1, // Color in shader specifies layout(location = 1) to
                         // link with this attribute.
          .binding = 0,  // Uses vertex buffer #0.
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = 4 * sizeof(float),
      },
  };

  VkPipelineVertexInputStateCreateInfo vertexInput = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &binding,
      .vertexAttributeDescriptionCount = 2,
      .pVertexAttributeDescriptions = attributes,
  };

  // Specify rasterization state.
  VkPipelineRasterizationStateCreateInfo raster = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = false,
      .rasterizerDiscardEnable = false,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = false,
      .lineWidth = 1.0f,
  };

  // Our attachment will write to all color channels, but no blending is
  // enabled.
  VkPipelineColorBlendAttachmentState blendAttachment = {
      .blendEnable = false,
      .colorWriteMask = 0xf,
  };

  VkPipelineColorBlendStateCreateInfo blend = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &blendAttachment,
  };

  // No multisampling.
  VkPipelineMultisampleStateCreateInfo multisample = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };

  // We will have one viewport and scissor box.
  VkPipelineViewportStateCreateInfo viewport = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1,
  };

  // Disable all depth testing.
  VkPipelineDepthStencilStateCreateInfo depthStencil = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = false,
      .depthWriteEnable = false,
      .depthBoundsTestEnable = false,
      .stencilTestEnable = false,
  };

  // Specify that these states will be dynamic, i.e. not part of pipeline state
  // object.
  static const VkDynamicState dynamics[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };
  VkPipelineDynamicStateCreateInfo dynamic = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = sizeof(dynamics) / sizeof(dynamics[0]),
      .pDynamicStates = dynamics,
  };

  // Create a blank pipeline layout.
  // We are not binding any resources to the pipeline in this first sample.
  VkPipelineLayout pipelineLayout;
  VkPipelineLayoutCreateInfo layoutInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
  };
  VK_CHECK(
      vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));

  // Specify we will use triangle lists to draw geometry.
  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .flags = 0,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = false,
  };

  VkGraphicsPipelineCreateInfo pipe = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = shaderStages,
      .pVertexInputState = &vertexInput,
      .pInputAssemblyState = &inputAssembly,
      .pViewportState = &viewport,
      .pRasterizationState = &raster,
      .pMultisampleState = &multisample,
      .pDepthStencilState = &depthStencil,
      .pColorBlendState = &blend,
      .pDynamicState = &dynamic,
      .layout = pipelineLayout,
      .renderPass = renderPass,
  };

  VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipe, nullptr,
                                     &pipeline));

  // Pipeline is baked, we can delete the shader modules now.
  vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
  vkDestroyShaderModule(device, shaderStages[1].module, nullptr);

  auto ptr = std::shared_ptr<Pipeline>(new Pipeline(
      device, renderPass, pipelineLayout, pipeline, pipelineCache));

  ptr->initVertexBuffer(props);

  return ptr;
}

void Pipeline::render(VkCommandBuffer cmd, VkFramebuffer framebuffer,
                      uint32_t width, uint32_t height) {

  // Set clear color values.
  VkClearValue clearValue{
      .color =
          {
              .float32 =
                  {
                      0.1f,
                      0.1f,
                      0.2f,
                      1.0f,
                  },
          },
  };

  // Begin the render pass.
  VkRenderPassBeginInfo rpBegin = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = _renderPass,
      .framebuffer = framebuffer,
      .renderArea = {.extent = {.width = width, .height = height}},
      .clearValueCount = 1,
      .pClearValues = &clearValue,
  };

  // We will add draw commands in the same command buffer.
  vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

  // Bind the graphics pipeline.
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

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
}

void Pipeline::initVertexBuffer(const VkPhysicalDeviceMemoryProperties &props) {
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
  vertexBuffer = createBuffer(_device, props, data, sizeof(data),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

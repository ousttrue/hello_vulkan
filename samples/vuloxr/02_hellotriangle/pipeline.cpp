#include "pipeline.hpp"
#include "../glsl_to_spv.h"
#include <vko/vko_pipeline.h>

struct Vertex {
  float position[4];
  float color[4];
};

class Buffer {
  VkDevice _device;
  VkBuffer _buffer;
  VkDeviceMemory _memory;

public:
  Buffer(VkDevice device, const VkPhysicalDeviceMemoryProperties &props,
         const void *pInitialData, size_t size, VkFlags usage)
      : _device(device) {
    VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
    };

    if (vkCreateBuffer(device, &info, nullptr, &_buffer) != VK_SUCCESS) {
      vko::Logger::Error("vkCreateBuffer");
      abort();
    }

    // Ask device about its memory requirements.
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, _buffer, &memReqs);

    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        // We want host visible and coherent memory to simplify things.
        .memoryTypeIndex = findMemoryTypeFromRequirements(
            props, memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };

    // Allocate memory.
    if (vkAllocateMemory(device, &alloc, nullptr, &_memory) != VK_SUCCESS) {
      vko::Logger::Error("vkAllocateMemory");
      abort();
    }

    // Buffers are not backed by memory, so bind our memory explicitly to the
    // buffer.
    vkBindBufferMemory(device, _buffer, _memory, 0);

    // Map the memory and dump data in there.
    if (pInitialData) {
      void *pData;
      if (vkMapMemory(device, _memory, 0, size, 0, &pData) != VK_SUCCESS) {
        vko::Logger::Error("vkMapMemory");
        abort();
      }
      memcpy(pData, pInitialData, size);
      vkUnmapMemory(device, _memory);
    }
  }

  ~Buffer() {
    vkFreeMemory(_device, _memory, nullptr);
    vkDestroyBuffer(_device, _buffer, nullptr);
  }

  void bind(VkCommandBuffer cmd, VkDeviceSize offset) {
    vkCmdBindVertexBuffers(cmd, 0, 1, &_buffer, &offset);
  }

private:
  // To create a buffer, both the device and application have requirements from
  // the buffer object.
  // Vulkan exposes the different types of buffers the device can allocate, and
  // we have to find a suitable one. deviceRequirements is a bitmask expressing
  // which memory types can be used for a buffer object. The different memory
  // types' properties must match with what the application wants.
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

    vko::Logger::Error("Failed to obtain suitable memory type.\n");
    abort();
  }
};

///
/// Pipeline
///
Pipeline::Pipeline(VkDevice device, VkPipelineLayout pipelineLayout,
                   VkPipeline pipeline, VkPipelineCache pipelineCache)
    : _device(device), _pipelineLayout(pipelineLayout), _pipeline(pipeline),
      _pipelineCache(pipelineCache) {}

Pipeline::~Pipeline() {
  _vertexBuffer = {};

  vkDestroyPipelineCache(_device, _pipelineCache, nullptr);
  vkDestroyPipeline(_device, _pipeline, nullptr);
  vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
}

std::shared_ptr<Pipeline> Pipeline::create(VkPhysicalDevice physicalDevice,
                                           VkDevice device,
                                           VkRenderPass renderPass,
                                           VkPipelineLayout pipelineLayout) {

  //
  // Pipeline
  //
  VkPipelineCache pipelineCache;
  VkPipelineCacheCreateInfo pipelineCacheInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
  };
  if (vkCreatePipelineCache(device, &pipelineCacheInfo, nullptr,
                            &pipelineCache) != VK_SUCCESS) {
    vko::Logger::Error("vkCreatePipelineCache");
    abort();
  }

  // Load our SPIR-V shaders.
  const char VS[] = {
#embed "triangle.vert"
      , 0, 0, 0, 0};

  const char FS[] = {
#embed "triangle.frag"
      , 0, 0, 0, 0};

  auto vs =
      vko::ShaderModule::createVertexShader(device, glsl_vs_to_spv(VS), "main");
  auto fs = vko::ShaderModule::createFragmentShader(device, glsl_fs_to_spv(FS),
                                                    "main");

  VkPipelineShaderStageCreateInfo shaderStages[2] = {
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          // We have two pipeline stages, vertex and fragment.
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = vs.shaderModule,
          .pName = "main",
      },
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = fs.shaderModule,
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

  if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipe, nullptr,
                                &pipeline) != VK_SUCCESS) {
    vko::Logger::Error("vkCreateGraphicsPipelines");
    return {};
  }

  auto ptr = std::shared_ptr<Pipeline>(
      new Pipeline(device, pipelineLayout, pipeline, pipelineCache));

  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &props);
  ptr->initVertexBuffer(props);

  return ptr;
}

void Pipeline::render(VkCommandBuffer cmd, VkFramebuffer framebuffer,
                      VkExtent2D size) {

  // Bind the graphics pipeline.
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

  // Set up dynamic state.
  // Viewport
  VkViewport vp = {0};
  vp.x = 0.0f;
  vp.y = 0.0f;
  vp.width = float(size.width);
  vp.height = float(size.height);
  vp.minDepth = 0.0f;
  vp.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &vp);

  // Scissor box
  VkRect2D scissor;
  memset(&scissor, 0, sizeof(scissor));
  scissor.extent = size;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // Bind vertex buffer.
  _vertexBuffer->bind(cmd, 0);

  // Draw three vertices with one instance.
  vkCmdDraw(cmd, 3, 1, 0, 0);
}

void Pipeline::initVertexBuffer(const VkPhysicalDeviceMemoryProperties &props) {
  // A simple counter-clockwise triangle.
  // We specify the positions directly in clip space.
  static const Vertex data[] = {
      {
          {-0.5f, -0.5f, 0.0f, 1.0f},
          {1.0f, 0.0f, 0.0f, 1.0f},
      },
      {
          {-0.5f, +0.5f, 0.0f, 1.0f},
          {0.0f, 1.0f, 0.0f, 1.0f},
      },
      {
          {+0.5f, -0.5f, 0.0f, 1.0f},
          {0.0f, 0.0f, 1.0f, 1.0f},
      },
  };

  // We will use the buffer as a vertex buffer only.
  _vertexBuffer = std::make_shared<Buffer>(_device, props, data, sizeof(data),
                                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

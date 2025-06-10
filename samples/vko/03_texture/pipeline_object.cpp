#include "pipeline_object.h"
#include "../glsl_to_spv.h"
#include "memory_allocator.h"
#include <array>
#include <fstream>
#include <glm/fwd.hpp>
#include <stdexcept>
#include <vko/vko_pipeline.h>
#include <vulkan/vulkan_core.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> //<-- includes functions such as ones that allow rotation, and perspective

auto VS = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main()
{
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0); 
    fragColor = inColor; 
    fragTexCoord = inTexCoord;
}
)";

auto FS = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(texSampler, fragTexCoord);
}
)";

struct DescriptorSetLayout {
  VkDevice _device;
  VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;

  VkDescriptorSetLayoutBinding bindings[2] = {
      {
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .pImmutableSamplers = nullptr,
      },
      {
          .binding = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          // indicate that we want to use the combined image sampler
          // descriptor in the fragment shader
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .pImmutableSamplers = nullptr,
      },
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(std::size(bindings)),
      .pBindings = bindings,
  };

  // VkDescriptorSetLayout descriptorSetLayout;
  DescriptorSetLayout(VkDevice device) : _device(device) {
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                    &_descriptorSetLayout) != VK_SUCCESS) {
      throw std::runtime_error("failed to create descriptor set layout!");
    }
  }

  static std::shared_ptr<DescriptorSetLayout> create(VkDevice device) {
    auto ptr =
        std::shared_ptr<DescriptorSetLayout>(new DescriptorSetLayout(device));
    return ptr;
  }

  ~DescriptorSetLayout() {
    vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, nullptr);
  }
};
static void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                                  VkFormat format, VkImageLayout oldLayout,
                                  VkImageLayout newLayout) {

  VkImageMemoryBarrier barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      // can also use VK_IMAGE_LAYOUT_UNDEFINED if we don't care about the
      // existing contents of image!
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;
  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
}

void UniformBufferObject::setTime(float time, float width, float height) {
  // time * radians(90.0f) = rotation of 90 degrees per second!
  *((glm::mat4 *)&this->model) = glm::rotate(
      glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  // specify view position
  *((glm::mat4 *)&this->view) =
      glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f));
  // view from a 45 degree angle
  *((glm::mat4 *)&this->proj) =
      glm::perspective(glm::radians(45.0f), width / height, 1.0f, 10.0f);

  // GLM was designed for OpenGL, therefor Y-coordinate (of the clip
  // coodinates) is inverted, the following code flips this around!
  this->proj.m[5] /*[1][1]*/ *= -1;
}

static std::vector<char> readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  return buffer;
}

static VkShaderModule createShaderModule(VkDevice device,
                                         const std::vector<uint32_t> &code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size() * 4;
  createInfo.pCode = code.data();

  VkShaderModule shaderModule;

  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create shader module!");
  }

  return shaderModule;
}

// Interleaving vertex attributes - includes position AND color attributes!
const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // top-left and RED
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},  // top-right and GREEN
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},   // bottom-right and BLUE
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}   // bottom-left and WHITE
};

const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

static VkVertexInputBindingDescription Vertex_getBindingDescription() {
  VkVertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0; // specify index of the binding
  bindingDescription.stride =
      sizeof(Vertex); // number of bytes from one entry to the next
  bindingDescription.inputRate =
      VK_VERTEX_INPUT_RATE_VERTEX; // Move to next data entry after each
                                   // vertex

  return bindingDescription;
}

PipelineObject::PipelineObject(
    VkDevice device,
    const std::shared_ptr<DescriptorSetLayout> &descriptorSetLayout,
    VkPipelineLayout pipelineLayout, VkRenderPass renderPass)
    : _device(device), _descriptorSetLayout(descriptorSetLayout),
      _pipelineLayout(pipelineLayout), _renderPass(renderPass) {}

PipelineObject::~PipelineObject() {
  vkDestroyRenderPass(_device, _renderPass, nullptr);

  if (_graphicsPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
    _graphicsPipeline = VK_NULL_HANDLE;
  }

  vkDestroySampler(_device, _textureSampler, nullptr);
  vkDestroyImageView(_device, _textureImageView, nullptr);
  _texture = {};

  vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
}

static void copyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer,
                              VkImage image, uint32_t width, uint32_t height) {
  VkBufferImageCopy region{
      .bufferOffset = 0,
      // functions as "padding" for the image destination
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .mipLevel = 0,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
      .imageOffset = {0, 0, 0},
      .imageExtent = {width, height, 1},
  };
  vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

std::shared_ptr<PipelineObject>
PipelineObject::create(VkPhysicalDevice physicalDevice, VkDevice device,
                       uint32_t graphicsQueueFamilyIndex,
                       VkFormat swapchainFormat) {
  auto descriptorSetLayout = DescriptorSetLayout::create(device);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptorSetLayout->_descriptorSetLayout,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };

  VkPipelineLayout pipelineLayout;
  if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                             &pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }

  auto renderPass = vko::createSimpleRenderPass(device, swapchainFormat);

  auto ptr = std::shared_ptr<PipelineObject>(new PipelineObject(
      device, descriptorSetLayout, pipelineLayout, renderPass));

  auto memory = std::make_shared<MemoryAllocator>(physicalDevice, device,
                                                  graphicsQueueFamilyIndex);

  {
    // ptr->createTextureImage(memory);
    int texWidth = 2;
    int texHeight = 2;
    int texChannels = 4;
    uint8_t pixels[] = {
        255, 0,   0,   255, // R
        0,   255, 0,   255, // G
        0,   0,   255, 255, // B
        255, 255, 255, 255, // WHITE
    };
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    auto stagingBuffer = std::make_shared<BufferObject>(
        physicalDevice, device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    stagingBuffer->copy(pixels);
    ptr->_texture = memory->createImage(
        texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    memory->oneTimeCommandSync([self = ptr](auto commandBuffer) {
      transitionImageLayout(commandBuffer, self->_texture->image(),
                            VK_FORMAT_R8G8B8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    });

    memory->oneTimeCommandSync(

        [self = ptr, stagingBuffer, texWidth, texHeight](auto commandBuffer) {
          copyBufferToImage(commandBuffer, stagingBuffer->buffer(),
                            self->_texture->image(),
                            static_cast<uint32_t>(texWidth),
                            static_cast<uint32_t>(texHeight));
        });

    memory->oneTimeCommandSync([self = ptr](auto commandBuffer) {
      transitionImageLayout(commandBuffer, self->_texture->image(),
                            VK_FORMAT_R8G8B8A8_SRGB,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });
  }
  {
    // ptr->createTextureImageView();
    // ptr->_textureImageView =
    //     ptr->createImageView(ptr->_texture->image(),
    //     VK_FORMAT_R8G8B8A8_SRGB);
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = ptr->_texture->image();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB; // format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    // VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr,
                          &ptr->_textureImageView) != VK_SUCCESS) {
      throw std::runtime_error("failed to create texture image view!");
    }

    // return imageView;
  }
  {
    // ptr->createTextureSampler();
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    // next two lines describe how to interpolate texels that are magnified or
    // minified
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    // note: image axes are UVW (rather than XYZ)
    samplerInfo.addressModeU =
        VK_SAMPLER_ADDRESS_MODE_REPEAT; // repeat the texture when out of bounds
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &ptr->_textureSampler) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create texture sampler!");
    }
  }
  {
    // ptr->createVertexBuffer(memory);
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    auto stagingBuffer = std::make_shared<BufferObject>(
        physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    stagingBuffer->copy(vertices.data(), bufferSize);
    ptr->_vertexBuffer = std::make_shared<BufferObject>(
        physicalDevice, device, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    memory->oneTimeCommandSync([stagingBuffer,
                                vertexBuffer = ptr->_vertexBuffer,
                                bufferSize](auto commandBuffer) {
      stagingBuffer->copyTo(commandBuffer, vertexBuffer->buffer(), bufferSize);
    });
  }
  {
    // ptr->createIndexBuffer(memory);
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    auto stagingBuffer = std::make_shared<BufferObject>(
        physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    stagingBuffer->copy(indices.data(), bufferSize);

    ptr->_indexBuffer = std::make_shared<BufferObject>(
        physicalDevice, device, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    memory->oneTimeCommandSync([stagingBuffer, indexBuffer = ptr->_indexBuffer,
                                bufferSize](auto commandBuffer) {
      stagingBuffer->copyTo(commandBuffer, indexBuffer->buffer(), bufferSize);
    });
  }

  return ptr;
}

void PipelineObject::createGraphicsPipeline(VkExtent2D swapchainExtent) {
  if (_graphicsPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
    _graphicsPipeline = VK_NULL_HANDLE;
  }

  // auto vertShaderCode = readFile("shaders/vert.spv");
  VkShaderModule vertShaderModule =
      createShaderModule(_device, glsl_vs_to_spv(VS));
  // auto fragShaderCode = readFile("shaders/frag.spv");
  VkShaderModule fragShaderModule =
      createShaderModule(_device, glsl_fs_to_spv(FS));
  VkPipelineShaderStageCreateInfo shaderStages[] = {
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = vertShaderModule,
          .pName = "main",
      },
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = fragShaderModule,
          .pName = "main",
      },
  };

  auto bindingDescription = Vertex_getBindingDescription();
  // auto attributeDescriptions = Vertex_getAttributeDescriptions();
  VkVertexInputAttributeDescription attributeDescriptions[]{
      // describes position
      {
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = offsetof(Vertex, pos),
      },
      // describes color
      {
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = offsetof(Vertex, color),
      },
      // uv
      {
          .location = 2,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = offsetof(Vertex, texCoord),
      },
  };

  // This struct decribes the format of the vertex data:
  //    1. Bindings (spacing between data and if per-instance or per-vertex
  //    2. Attribute descriptions (what is passed to vertex shader)
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &bindingDescription,
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(std::size(attributeDescriptions)),
      .pVertexAttributeDescriptions = attributeDescriptions,
  };

  // This struct describes two things as well:
  //    1. Type of geometry drawn from vertices
  //    2. If primitive restart should be enabled
  VkPipelineInputAssemblyStateCreateInfo inputAssembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = (float)swapchainExtent.width,
      .height = (float)swapchainExtent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  VkRect2D scissor{
      .offset = {0, 0},
      .extent = swapchainExtent,
  };
  VkPipelineViewportStateCreateInfo viewportState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor,
  };

  // build the rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      // counter clockwise (to match Y-flip in the projection matrix)
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f, // optional
      .depthBiasClamp = 0.0f,          // optional
      .depthBiasSlopeFactor = 0.0f,    // optional
      .lineWidth = 1.0f,
  };

  // Multisampling
  VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0f,          // optional
      .pSampleMask = nullptr,            // optional
      .alphaToCoverageEnable = VK_FALSE, // optional
      .alphaToOneEnable = VK_FALSE,      // optional
  };

  // Color Blending
  VkPipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,  // optional
      .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO, // optional
      .colorBlendOp = VK_BLEND_OP_ADD,             // optional
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,  // optional
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, // optional
      .alphaBlendOp = VK_BLEND_OP_ADD,             // optional
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };
  // references the array of structures for all framebuffers and allows for
  // blend factor specification
  VkPipelineColorBlendStateCreateInfo colorBlending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY, // optional
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment,
      .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
  };

  VkGraphicsPipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = shaderStages,
      .pVertexInputState = &vertexInputInfo,
      .pInputAssemblyState = &inputAssembly,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = nullptr,
      .pColorBlendState = &colorBlending,
      .pDynamicState = nullptr,
      .layout = _pipelineLayout,
      .renderPass = _renderPass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
  };
  if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &_graphicsPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
  vkDestroyShaderModule(_device, fragShaderModule, nullptr);
  vkDestroyShaderModule(_device, vertShaderModule, nullptr);
}

void PipelineObject::record(VkCommandBuffer commandBuffer,
                            VkFramebuffer framebuffer, VkExtent2D extent,
                            VkDescriptorSet descriptorSet) {

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = 0,
      .pInheritanceInfo = nullptr,
  };
  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }

  VkRenderPassBeginInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = _renderPass,
      .framebuffer = framebuffer,
      .renderArea =
          {
              .offset = {0, 0},
              .extent = extent,
          },
      .clearValueCount = 1,
      .pClearValues = &_clearColor,
  };
  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _graphicsPipeline);

  VkBuffer vertexBuffers[] = {_vertexBuffer->buffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
  vkCmdBindIndexBuffer(commandBuffer, _indexBuffer->buffer(), 0,
                       VK_INDEX_TYPE_UINT16);

  // take the descriptor set for the corresponding swap image, and bind it
  // to the descriptors in the shader
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          _pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

  // Tell Vulkan to draw the triangle USING THE INDEX BUFFER!
  vkCmdDrawIndexed(commandBuffer, indices.size(), 1, 0, 0, 0);

  vkCmdEndRenderPass(commandBuffer);

  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }
}

VkDescriptorSetLayout PipelineObject::descriptorSetLayout() const {
  return _descriptorSetLayout->_descriptorSetLayout;
}

VkRenderPass PipelineObject::renderPass() const { return _renderPass; }

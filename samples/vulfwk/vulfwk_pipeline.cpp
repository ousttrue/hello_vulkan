#include "vulfwk_pipeline.h"
#include "glsl_to_spv.h"
#include <shaderc/shaderc.h>
#include <vko/vko.h>

#if ANDROID
#include <android_native_app_glue.h>
static std::vector<char> readFile(const char *filePath, void *p) {
  auto manager = reinterpret_cast<AAssetManager *>(p);
  AAsset *asset = AAssetManager_open(manager, filePath, AASSET_MODE_BUFFER);
  if (!asset) {
    vko::Logger::Error("failed to AAssetManager_open: %s", filePath);
    return {};
  }

  size_t size = AAsset_getLength(asset);
  std::vector<char> buffer(size);

  AAsset_read(asset, buffer.data(), size);
  AAsset_close(asset);

  return buffer;
}
#else
#include <fstream>
static std::vector<char> readFile(const char *filename, void *) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    vko::Logger::Error("failed to open: %s", filename);
    return {};
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  return buffer;
}
#endif

static VkShaderModule createShaderModule(VkDevice device,
                                         const std::vector<char> &code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    vko::Logger::Error("failed to create shader module!");
    return VK_NULL_HANDLE;
  }

  return shaderModule;
}

PipelineImpl::PipelineImpl(VkDevice device, VkRenderPass renderPass,
                           VkPipelineLayout pipelineLayout,
                           VkPipeline graphicsPipeline)
    : Device(device), RenderPass(renderPass), PipelineLayout(pipelineLayout),
      GraphicsPipeline(graphicsPipeline) {}

PipelineImpl::~PipelineImpl() {
  if (GraphicsPipeline) {
    vkDestroyPipeline(Device, GraphicsPipeline, nullptr);
  }
  if (PipelineLayout) {
    vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
  }
  if (RenderPass) {
    vkDestroyRenderPass(Device, RenderPass, nullptr);
  }
}

std::shared_ptr<PipelineImpl>

PipelineImpl::create(VkPhysicalDevice physicalDevice, VkDevice device,
                     VkFormat swapchainImageFormat, void *AssetManager) {
  auto renderPass = createRenderPass(device, swapchainImageFormat);
  if (renderPass == VK_NULL_HANDLE) {
    return {};
  }

  auto vertShaderSrc = readFile("shader.vert", AssetManager);
  if (vertShaderSrc.empty()) {
    return {};
  }
  auto vertShaderCode = glsl_vs_to_spv(vertShaderSrc.data(), vertShaderSrc.size());
  if (vertShaderCode.empty()) {
    return {};
  }

  auto fragShaderSrc = readFile("shader.frag", AssetManager);
  if (fragShaderSrc.empty()) {
    return {};
  }
  auto fragShaderCode = glsl_fs_to_spv(fragShaderSrc.data(), fragShaderSrc.size());
  if (fragShaderCode.empty()) {
    return {};
  }

  VkShaderModule vertShaderModule = createShaderModule(device, vertShaderCode);
  if (vertShaderModule == VK_NULL_HANDLE) {
    return {};
  }
  VkShaderModule fragShaderModule = createShaderModule(device, fragShaderCode);
  if (fragShaderModule == VK_NULL_HANDLE) {
    return {};
  }

  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                    fragShaderStageInfo};

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 0;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 0;
  pipelineLayoutInfo.pushConstantRangeCount = 0;

  VkPipelineLayout pipelineLayout;
  if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                             &pipelineLayout) != VK_SUCCESS) {
    vko::Logger::Error("failed to create pipeline layout!");
    return {};
  }

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.renderPass = renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  VkPipeline graphicsPipeline;
  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &graphicsPipeline) != VK_SUCCESS) {
    vko::Logger::Error("failed to create graphics pipeline!");
    return {};
  }

  vkDestroyShaderModule(device, fragShaderModule, nullptr);
  vkDestroyShaderModule(device, vertShaderModule, nullptr);

  auto p =
      new PipelineImpl(device, renderPass, pipelineLayout, graphicsPipeline);

  return std::shared_ptr<PipelineImpl>(p);
}

VkRenderPass PipelineImpl::createRenderPass(VkDevice device,
                                            VkFormat swapchainImageFormat) {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = swapchainImageFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  VkRenderPass renderPass;
  if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) !=
      VK_SUCCESS) {
    vko::Logger::Error("failed to create render pass!");
    return {};
  }

  return renderPass;
}

void PipelineImpl::draw(VkCommandBuffer commandBuffer, uint32_t imageIndex,
                        VkFramebuffer framebuffer, VkExtent2D imageExtent) {
  vkResetCommandBuffer(commandBuffer,
                       /*VkCommandBufferResetFlagBits*/ 0);
  recordCommandBuffer(commandBuffer, imageIndex, framebuffer, imageExtent);
}

bool PipelineImpl::recordCommandBuffer(VkCommandBuffer commandBuffer,
                                       uint32_t imageIndex,
                                       VkFramebuffer framebuffer,
                                       VkExtent2D imageExtent) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    vko::Logger::Error("failed to begin recording command buffer!");
    return false;
  }

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = RenderPass;
  renderPassInfo.framebuffer = framebuffer;
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = imageExtent;
  VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    GraphicsPipeline);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)imageExtent.width;
  viewport.height = (float)imageExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = imageExtent;
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  vkCmdDraw(commandBuffer, 3, 1, 0, 0);

  vkCmdEndRenderPass(commandBuffer);

  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    vko::Logger::Error("failed to record command buffer!");
    return false;
  }

  return true;
}

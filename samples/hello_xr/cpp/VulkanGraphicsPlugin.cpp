#include "VulkanGraphicsPlugin.h"
#include "CmdBuffer.h"
#include "DepthBuffer.h"
#include "MemoryAllocator.h"
#include "Pipeline.h"
#include "SwapchainImageContext.h"
#include "VertexBuffer.h"
#include "logger.h"
#include "openxr/openxr.h"
#include "options.h"
#include "to_string.h"
#include "vulkan_debug_object_namer.hpp"
#include <algorithm>
#include <stdexcept>

#include <common/xr_linear.h>

VulkanGraphicsPlugin::VulkanGraphicsPlugin(
    const Options &options, std::shared_ptr<struct IPlatformPlugin> /*unused*/)
    : m_clearColor(options.GetBackgroundClearColor()) {}

// Note: The output must not outlive the input - this modifies the input and
// returns a collection of views into that modified input!
std::vector<const char *>
VulkanGraphicsPlugin::ParseExtensionString(char *names) {
  std::vector<const char *> list;
  while (*names != 0) {
    list.push_back(names);
    while (*(++names) != 0) {
      if (*names == ' ') {
        *names++ = '\0';
        break;
      }
    }
  }
  return list;
}

void VulkanGraphicsPlugin::InitializeDevice(
    VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
    VkDeviceQueueCreateInfo queueInfo,
    VkDebugUtilsMessengerCreateInfoEXT debugInfo) {
  m_vkInstance = instance;
  m_vkPhysicalDevice = physicalDevice;
  m_vkDevice = device;
  m_queueFamilyIndex = queueInfo.queueFamilyIndex;

  vkCreateDebugUtilsMessengerEXT =
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          m_vkInstance, "vkCreateDebugUtilsMessengerEXT");
  if (vkCreateDebugUtilsMessengerEXT != nullptr) {
    if (vkCreateDebugUtilsMessengerEXT(m_vkInstance, &debugInfo, nullptr,
                                       &m_vkDebugUtilsMessenger) !=
        VK_SUCCESS) {
      throw std::runtime_error("vkCreateDebugUtilsMessengerEXT");
    }
  }

  vkGetDeviceQueue(m_vkDevice, queueInfo.queueFamilyIndex, 0, &m_vkQueue);

  m_memAllocator = MemoryAllocator::Create(m_vkPhysicalDevice, m_vkDevice);

  InitializeResources();
}

#ifdef USE_ONLINE_VULKAN_SHADERC
// Compile a shader to a SPIR-V binary.
std::vector<uint32_t>
VulkanGraphicsPlugin::CompileGlslShader(const std::string &name,
                                        shaderc_shader_kind kind,
                                        const std::string &source) {
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;

  options.SetOptimizationLevel(shaderc_optimization_level_size);

  shaderc::SpvCompilationResult module =
      compiler.CompileGlslToSpv(source, kind, name.c_str(), options);

  if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
    Log::Write(Log::Level::Error,
               Fmt("Shader %s compilation failed: %s", name.c_str(),
                   module.GetErrorMessage().c_str()));
    return std::vector<uint32_t>();
  }

  return {module.cbegin(), module.cend()};
}
#endif

// glslangValidator doesn't wrap its output in brackets if you don't have it
// define the whole array.
#if defined(USE_GLSLANGVALIDATOR)
#define SPV_PREFIX {
#define SPV_SUFFIX }
#else
#define SPV_PREFIX
#define SPV_SUFFIX
#endif

void VulkanGraphicsPlugin::InitializeResources() {
#ifdef USE_ONLINE_VULKAN_SHADERC
  auto vertexSPIRV = CompileGlslShader(
      "vertex", shaderc_glsl_default_vertex_shader, VertexShaderGlsl);
  auto fragmentSPIRV = CompileGlslShader(
      "fragment", shaderc_glsl_default_fragment_shader, FragmentShaderGlsl);
#else
  std::vector<uint32_t> vertexSPIRV = SPV_PREFIX
#include "vert.spv"
      SPV_SUFFIX;
  std::vector<uint32_t> fragmentSPIRV = SPV_PREFIX
#include "frag.spv"
      SPV_SUFFIX;
#endif
  if (vertexSPIRV.empty()) {
    throw std::runtime_error("Failed to compile vertex shader");
  }
  if (fragmentSPIRV.empty()) {
    throw std::runtime_error("Failed to compile fragment shader");
  }

  m_shaderProgram = ShaderProgram::Create(m_vkDevice);
  m_shaderProgram->LoadVertexShader(vertexSPIRV);
  m_shaderProgram->LoadFragmentShader(fragmentSPIRV);

  // Semaphore to block on draw complete
  VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  if (vkCreateSemaphore(m_vkDevice, &semInfo, nullptr, &m_vkDrawDone) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateSemaphore");
  }
  if (SetDebugUtilsObjectNameEXT(
          m_vkDevice, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)m_vkDrawDone,
          "hello_xr draw done semaphore") != VK_SUCCESS) {
    throw std::runtime_error("SetDebugUtilsObjectNameEXT");
  }

  m_cmdBuffer = CmdBuffer::Create(m_vkDevice, m_queueFamilyIndex);
  if (!m_cmdBuffer) {
    throw std::runtime_error("Failed to create command buffer");
  }

  m_pipelineLayout = PipelineLayout::Create(m_vkDevice);

  static_assert(sizeof(Vertex) == 24, "Unexpected Vertex size");

  m_drawBuffer = VertexBuffer::Create(
      m_vkDevice, m_memAllocator,
      {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position)},
       {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Color)}},
      c_cubeVertices, std::size(c_cubeVertices), c_cubeIndices,
      std::size(c_cubeIndices));

#if defined(USE_MIRROR_WINDOW)
  m_swapchain.Create(m_vkInstance, m_vkPhysicalDevice, m_vkDevice,
                     m_queueFamilyIndex);
  m_cmdBuffer.Reset();
  m_cmdBuffer.Begin();
  m_swapchain.Prepare(m_cmdBuffer.buf);
  m_cmdBuffer.End();
  m_cmdBuffer.Exec(m_vkQueue);
  m_cmdBuffer.Wait();
#endif
}

int64_t VulkanGraphicsPlugin::SelectColorSwapchainFormat(
    const std::vector<int64_t> &runtimeFormats) const {
  // List of supported color swapchain formats.
  constexpr int64_t SupportedColorSwapchainFormats[] = {
      VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB,
      VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};

  auto swapchainFormatIt =
      std::find_first_of(runtimeFormats.begin(), runtimeFormats.end(),
                         std::begin(SupportedColorSwapchainFormats),
                         std::end(SupportedColorSwapchainFormats));
  if (swapchainFormatIt == runtimeFormats.end()) {
    throw std::runtime_error(
        "No runtime swapchain format supported for color swapchain");
  }

  return *swapchainFormatIt;
}

std::vector<XrSwapchainImageBaseHeader *>
VulkanGraphicsPlugin::AllocateSwapchainImageStructs(
    uint32_t capacity, VkExtent2D size, VkFormat format,
    VkSampleCountFlagBits sampleCount) {
  // Allocate and initialize the buffer of image structs (must be sequential
  // in memory for xrEnumerateSwapchainImages). Return back an array of
  // pointers to each swapchain image struct so the consumer doesn't need to
  // know the type/size. Keep the buffer alive by adding it into the list of
  // buffers.
  m_swapchainImageContexts.emplace_back(
      SwapchainImageContext::create(XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR));
  auto swapchainImageContext = m_swapchainImageContexts.back();

  //
  std::vector<XrSwapchainImageBaseHeader *> bases =
      swapchainImageContext->Create(m_vkDevice, m_memAllocator, capacity, size,
                                    format, sampleCount, m_pipelineLayout,
                                    m_shaderProgram, m_drawBuffer);

  // Map every swapchainImage base pointer to this context
  for (auto &base : bases) {
    m_swapchainImageContextMap[base] = swapchainImageContext;
  }

  return bases;
}

void VulkanGraphicsPlugin::RenderView(
    const XrCompositionLayerProjectionView &layerView,
    const XrSwapchainImageBaseHeader *swapchainImage,
    int64_t /*swapchainFormat*/, const std::vector<Cube> &cubes) {
  // CHECK(layerView.subImage.imageArrayIndex ==
  //       0); // Texture arrays not supported.

  auto swapchainContext = m_swapchainImageContextMap[swapchainImage];
  uint32_t imageIndex = swapchainContext->ImageIndex(swapchainImage);

  // XXX Should double-buffer the command buffers, for now just flush
  m_cmdBuffer->Wait();
  m_cmdBuffer->Reset();
  m_cmdBuffer->Begin();

  // Ensure depth is in the right layout
  swapchainContext->depthBuffer->TransitionLayout(
      m_cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  // Bind and clear eye render target
  static std::array<VkClearValue, 2> clearValues;
  clearValues[0].color.float32[0] = m_clearColor[0];
  clearValues[0].color.float32[1] = m_clearColor[1];
  clearValues[0].color.float32[2] = m_clearColor[2];
  clearValues[0].color.float32[3] = m_clearColor[3];
  clearValues[1].depthStencil.depth = 1.0f;
  clearValues[1].depthStencil.stencil = 0;
  VkRenderPassBeginInfo renderPassBeginInfo{
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  renderPassBeginInfo.clearValueCount = (uint32_t)clearValues.size();
  renderPassBeginInfo.pClearValues = clearValues.data();

  swapchainContext->BindRenderTarget(imageIndex, &renderPassBeginInfo);

  vkCmdBeginRenderPass(m_cmdBuffer->buf, &renderPassBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(m_cmdBuffer->buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    swapchainContext->m_pipe->pipe);

  // Bind index and vertex buffers
  vkCmdBindIndexBuffer(m_cmdBuffer->buf, m_drawBuffer->idxBuf, 0,
                       VK_INDEX_TYPE_UINT16);
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(m_cmdBuffer->buf, 0, 1, &m_drawBuffer->vtxBuf,
                         &offset);

  // Compute the view-projection transform.
  // Note all matrixes (including OpenXR's) are column-major, right-handed.
  const auto &pose = layerView.pose;
  XrMatrix4x4f proj;
  XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_VULKAN, layerView.fov, 0.05f,
                                   100.0f);
  XrMatrix4x4f toView;
  XrMatrix4x4f_CreateFromRigidTransform(&toView, &pose);
  XrMatrix4x4f view;
  XrMatrix4x4f_InvertRigidBody(&view, &toView);
  XrMatrix4x4f vp;
  XrMatrix4x4f_Multiply(&vp, &proj, &view);

  // Render each cube
  for (const Cube &cube : cubes) {
    // Compute the model-view-projection transform and push it.
    XrMatrix4x4f model;
    XrMatrix4x4f_CreateTranslationRotationScale(
        &model, (XrVector3f *)&cube.Translaton, (XrQuaternionf *)&cube.Rotation,
        (XrVector3f *)&cube.Scaling);
    XrMatrix4x4f mvp;
    XrMatrix4x4f_Multiply(&mvp, &vp, &model);
    vkCmdPushConstants(m_cmdBuffer->buf, m_pipelineLayout->layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvp.m), &mvp.m[0]);

    // Draw the cube.
    vkCmdDrawIndexed(m_cmdBuffer->buf, m_drawBuffer->count.idx, 1, 0, 0, 0);
  }

  vkCmdEndRenderPass(m_cmdBuffer->buf);

  m_cmdBuffer->End();
  m_cmdBuffer->Exec(m_vkQueue);

#if defined(USE_MIRROR_WINDOW)
  // Cycle the window's swapchain on the last view rendered
  if (swapchainContext == &m_swapchainImageContexts.back()) {
    m_swapchain.Acquire();
    m_swapchain.Wait();
    m_swapchain.Present(m_vkQueue);
  }
#endif
}

void VulkanGraphicsPlugin::UpdateOptions(const Options &options) {
  m_clearColor = options.GetBackgroundClearColor();
}

static std::string vkObjectTypeToString(VkObjectType objectType) {
  std::string objName;

#define LIST_OBJECT_TYPES(_)                                                   \
  _(UNKNOWN)                                                                   \
  _(INSTANCE)                                                                  \
  _(PHYSICAL_DEVICE)                                                           \
  _(DEVICE)                                                                    \
  _(QUEUE)                                                                     \
  _(SEMAPHORE)                                                                 \
  _(COMMAND_BUFFER)                                                            \
  _(FENCE)                                                                     \
  _(DEVICE_MEMORY)                                                             \
  _(BUFFER)                                                                    \
  _(IMAGE)                                                                     \
  _(EVENT)                                                                     \
  _(QUERY_POOL)                                                                \
  _(BUFFER_VIEW)                                                               \
  _(IMAGE_VIEW)                                                                \
  _(SHADER_MODULE)                                                             \
  _(PIPELINE_CACHE)                                                            \
  _(PIPELINE_LAYOUT)                                                           \
  _(RENDER_PASS)                                                               \
  _(PIPELINE)                                                                  \
  _(DESCRIPTOR_SET_LAYOUT)                                                     \
  _(SAMPLER)                                                                   \
  _(DESCRIPTOR_POOL)                                                           \
  _(DESCRIPTOR_SET)                                                            \
  _(FRAMEBUFFER)                                                               \
  _(COMMAND_POOL)                                                              \
  _(SURFACE_KHR)                                                               \
  _(SWAPCHAIN_KHR)                                                             \
  _(DISPLAY_KHR)                                                               \
  _(DISPLAY_MODE_KHR)                                                          \
  _(DESCRIPTOR_UPDATE_TEMPLATE_KHR)                                            \
  _(DEBUG_UTILS_MESSENGER_EXT)

  switch (objectType) {
  default:
#define MK_OBJECT_TYPE_CASE(name)                                              \
  case VK_OBJECT_TYPE_##name:                                                  \
    objName = #name;                                                           \
    break;
    LIST_OBJECT_TYPES(MK_OBJECT_TYPE_CASE)
  }

  return objName;
}

VkBool32 VulkanGraphicsPlugin::debugMessage(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData) {
  std::string flagNames;
  std::string objName;
  Log::Level level = Log::Level::Error;

  if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) !=
      0u) {
    flagNames += "DEBUG:";
    level = Log::Level::Verbose;
  }
  if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0u) {
    flagNames += "INFO:";
    level = Log::Level::Info;
  }
  if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) !=
      0u) {
    flagNames += "WARN:";
    level = Log::Level::Warning;
  }
  if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0u) {
    flagNames += "ERROR:";
    level = Log::Level::Error;
  }
  if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0u) {
    flagNames += "PERF:";
    level = Log::Level::Warning;
  }

  uint64_t object = 0;
  // skip loader messages about device extensions
  if (pCallbackData->objectCount > 0) {
    auto objectType = pCallbackData->pObjects[0].objectType;
    if ((objectType == VK_OBJECT_TYPE_INSTANCE) &&
        (strncmp(pCallbackData->pMessage, "Device Extension:", 17) == 0)) {
      return VK_FALSE;
    }
    objName = vkObjectTypeToString(objectType);
    object = pCallbackData->pObjects[0].objectHandle;
    if (pCallbackData->pObjects[0].pObjectName != nullptr) {
      objName += " " + std::string(pCallbackData->pObjects[0].pObjectName);
    }
  }

  Log::Write(level, Fmt("%s (%s 0x%llx) %s", flagNames.c_str(), objName.c_str(),
                        object, pCallbackData->pMessage));

  return VK_FALSE;
}

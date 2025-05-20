// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
#include "CmdBuffer.h"
#include "CreateGraphicsPlugin_Vulkan.h"
#include "DepthBuffer.h"
#include "MemoryAllocator.h"
#include "Pipeline.h"
#include "RenderPass.h"
#include "SwapchainImageContext.h"
#include "VertexBuffer.h"
#include "vulkan_debug_object_namer.hpp"

#include <stdexcept>
#include <vulkan/vulkan_core.h>
#ifdef XR_USE_PLATFORM_WIN32
#include <unknwn.h>
#include <windows.h>
#endif

#include "check.h"
#include "graphicsplugin_vulkan.h"
#include "logger.h"
#include "options.h"

#include "vk_utils.h"
#include <vulkan/vulkan.h>

#ifdef XR_USE_PLATFORM_ANDROID
#include <android_native_app_glue.h>
#endif

#include <openxr/openxr_platform.h>

#include <algorithm>
#include <list>
#include <map>

#ifdef XR_USE_GRAPHICS_API_VULKAN
#include <common/xr_linear.h>

#ifdef USE_ONLINE_VULKAN_SHADERC
#include <shaderc/shaderc.hpp>
#endif

#if defined(VK_USE_PLATFORM_WIN32_KHR)
// Define USE_MIRROR_WINDOW to open a otherwise-unused window for e.g. RenderDoc
#define USE_MIRROR_WINDOW
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

struct VulkanGraphicsPlugin : public IGraphicsPlugin {
  VulkanGraphicsPlugin(const std::shared_ptr<Options> &options,
                       std::shared_ptr<struct IPlatformPlugin> /*unused*/)
      : m_clearColor(options->GetBackgroundClearColor()) {
    m_graphicsBinding.type = GetGraphicsBindingType();
  };

  std::vector<std::string> GetInstanceExtensions() const override {
    return {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
  }

  // Note: The output must not outlive the input - this modifies the input and
  // returns a collection of views into that modified input!
  std::vector<const char *> ParseExtensionString(char *names) {
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

  const char *GetValidationLayerName() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    std::vector<const char *> validationLayerNames;
    validationLayerNames.push_back("VK_LAYER_KHRONOS_validation");
    validationLayerNames.push_back("VK_LAYER_LUNARG_standard_validation");

    // Enable only one validation layer from the list above. Prefer KHRONOS.
    for (auto &validationLayerName : validationLayerNames) {
      for (const auto &layerProperties : availableLayers) {
        if (0 == strcmp(validationLayerName, layerProperties.layerName)) {
          return validationLayerName;
        }
      }
    }

    return nullptr;
  }

  void InitializeDevice(XrInstance instance, XrSystemId systemId) override {
    // Create the Vulkan device for the adapter associated with the system.
    // Extension function must be loaded by name
    XrGraphicsRequirementsVulkan2KHR graphicsRequirements{
        XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    CHECK_XRCMD(GetVulkanGraphicsRequirements2KHR(instance, systemId,
                                                  &graphicsRequirements));

    VkResult err;

    std::vector<const char *> layers;
#if !defined(NDEBUG)
    const char *const validationLayerName = GetValidationLayerName();
    if (validationLayerName) {
      layers.push_back(validationLayerName);
    } else {
      Log::Write(Log::Level::Warning,
                 "No validation layers found in the system, skipping");
    }
#endif

    std::vector<const char *> extensions;
    {
      uint32_t extensionCount = 0;
      if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                                 nullptr) != VK_SUCCESS) {
        throw std::runtime_error("vkEnumerateInstanceExtensionProperties");
      }

      std::vector<VkExtensionProperties> availableExtensions(extensionCount);
      if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                                 availableExtensions.data()) !=
          VK_SUCCESS) {
        throw std::runtime_error("vkEnumerateInstanceExtensionProperties");
      }
      const auto b = availableExtensions.begin();
      const auto e = availableExtensions.end();

      auto isExtSupported = [&](const char *extName) -> bool {
        auto it =
            std::find_if(b, e, [&](const VkExtensionProperties &properties) {
              return (0 == strcmp(extName, properties.extensionName));
            });
        return (it != e);
      };

      // Debug utils is optional and not always available
      if (isExtSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
      }
      // TODO add back VK_EXT_debug_report code for compatibility with older
      // systems? (Android)
    }
#if defined(USE_MIRROR_WINDOW)
    extensions.push_back("VK_KHR_surface");
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    extensions.push_back("VK_KHR_win32_surface");
#else
#error CreateSurface not supported on this OS
#endif // defined(VK_USE_PLATFORM_WIN32_KHR)
#endif // defined(USE_MIRROR_WINDOW)

    VkDebugUtilsMessengerCreateInfoEXT debugInfo{
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
#if !defined(NDEBUG)
    debugInfo.messageSeverity |=
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
#endif
    debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugInfo.pfnUserCallback = debugMessageThunk;
    debugInfo.pUserData = this;

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "hello_xr";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "hello_xr";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instInfo.pNext = &debugInfo;
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledLayerCount = (uint32_t)layers.size();
    instInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
    instInfo.enabledExtensionCount = (uint32_t)extensions.size();
    instInfo.ppEnabledExtensionNames =
        extensions.empty() ? nullptr : extensions.data();

    XrVulkanInstanceCreateInfoKHR createInfo{
        XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    createInfo.systemId = systemId;
    createInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
    createInfo.vulkanCreateInfo = &instInfo;
    createInfo.vulkanAllocator = nullptr;
    CHECK_XRCMD(
        CreateVulkanInstanceKHR(instance, &createInfo, &m_vkInstance, &err));
    if (err != VK_SUCCESS) {
      throw std::runtime_error("CreateVulkanInstanceKHR");
    }
    SetDebugUtilsObjectNameEXT_GetProc(m_vkInstance);

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

    XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo{
        XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    deviceGetInfo.systemId = systemId;
    deviceGetInfo.vulkanInstance = m_vkInstance;
    CHECK_XRCMD(GetVulkanGraphicsDevice2KHR(instance, &deviceGetInfo,
                                            &m_vkPhysicalDevice));

    VkDeviceQueueCreateInfo queueInfo{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    float queuePriorities = 0;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriorities;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice,
                                             &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_vkPhysicalDevice, &queueFamilyCount, &queueFamilyProps[0]);

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
      // Only need graphics (not presentation) for draw queue
      if ((queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
        m_queueFamilyIndex = queueInfo.queueFamilyIndex = i;
        break;
      }
    }

    std::vector<const char *> deviceExtensions;

    VkPhysicalDeviceFeatures features{};
    // features.samplerAnisotropy = VK_TRUE;

#if defined(USE_MIRROR_WINDOW)
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#endif

    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledLayerCount = 0;
    deviceInfo.ppEnabledLayerNames = nullptr;
    deviceInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    deviceInfo.ppEnabledExtensionNames =
        deviceExtensions.empty() ? nullptr : deviceExtensions.data();
    deviceInfo.pEnabledFeatures = &features;

    XrVulkanDeviceCreateInfoKHR deviceCreateInfo{
        XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    deviceCreateInfo.systemId = systemId;
    deviceCreateInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
    deviceCreateInfo.vulkanCreateInfo = &deviceInfo;
    deviceCreateInfo.vulkanPhysicalDevice = m_vkPhysicalDevice;
    deviceCreateInfo.vulkanAllocator = nullptr;
    CHECK_XRCMD(
        CreateVulkanDeviceKHR(instance, &deviceCreateInfo, &m_vkDevice, &err));
    if (err != VK_SUCCESS) {
      throw std::runtime_error("CreateVulkanDeviceKHR");
    }

    vkGetDeviceQueue(m_vkDevice, queueInfo.queueFamilyIndex, 0, &m_vkQueue);

    m_memAllocator = MemoryAllocator::Create(m_vkPhysicalDevice, m_vkDevice);

    InitializeResources();

    m_graphicsBinding.instance = m_vkInstance;
    m_graphicsBinding.physicalDevice = m_vkPhysicalDevice;
    m_graphicsBinding.device = m_vkDevice;
    m_graphicsBinding.queueFamilyIndex = queueInfo.queueFamilyIndex;
    m_graphicsBinding.queueIndex = 0;
  }

#ifdef USE_ONLINE_VULKAN_SHADERC
  // Compile a shader to a SPIR-V binary.
  std::vector<uint32_t> CompileGlslShader(const std::string &name,
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

  void InitializeResources() {
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
    if (vertexSPIRV.empty())
      THROW("Failed to compile vertex shader");
    if (fragmentSPIRV.empty())
      THROW("Failed to compile fragment shader");

    m_shaderProgram.Init(m_vkDevice);
    m_shaderProgram.LoadVertexShader(vertexSPIRV);
    m_shaderProgram.LoadFragmentShader(fragmentSPIRV);

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

    if (!m_cmdBuffer.Init(m_vkDevice, m_queueFamilyIndex))
      THROW("Failed to create command buffer");

    m_pipelineLayout.Create(m_vkDevice);

    static_assert(sizeof(Vertex) == 24, "Unexpected Vertex size");

    m_drawBuffer = VertexBuffer::Create(
        m_vkDevice, m_memAllocator,
        {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position)},
         {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Color)}},
        c_cubeVertices, _countof(c_cubeVertices), c_cubeIndices,
        _countof(c_cubeIndices));

#if defined(USE_MIRROR_WINDOW)
    m_swapchain.Create(m_vkInstance, m_vkPhysicalDevice, m_vkDevice,
                       m_graphicsBinding.queueFamilyIndex);

    m_cmdBuffer.Reset();
    m_cmdBuffer.Begin();
    m_swapchain.Prepare(m_cmdBuffer.buf);
    m_cmdBuffer.End();
    m_cmdBuffer.Exec(m_vkQueue);
    m_cmdBuffer.Wait();
#endif
  }

  int64_t SelectColorSwapchainFormat(
      const std::vector<int64_t> &runtimeFormats) const override {
    // List of supported color swapchain formats.
    constexpr int64_t SupportedColorSwapchainFormats[] = {
        VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};

    auto swapchainFormatIt =
        std::find_first_of(runtimeFormats.begin(), runtimeFormats.end(),
                           std::begin(SupportedColorSwapchainFormats),
                           std::end(SupportedColorSwapchainFormats));
    if (swapchainFormatIt == runtimeFormats.end()) {
      THROW("No runtime swapchain format supported for color swapchain");
    }

    return *swapchainFormatIt;
  }

  const XrBaseInStructure *GetGraphicsBinding() const override {
    return reinterpret_cast<const XrBaseInStructure *>(&m_graphicsBinding);
  }

  std::vector<XrSwapchainImageBaseHeader *> AllocateSwapchainImageStructs(
      uint32_t capacity,
      const XrSwapchainCreateInfo &swapchainCreateInfo) override {
    // Allocate and initialize the buffer of image structs (must be sequential
    // in memory for xrEnumerateSwapchainImages). Return back an array of
    // pointers to each swapchain image struct so the consumer doesn't need to
    // know the type/size. Keep the buffer alive by adding it into the list of
    // buffers.
    m_swapchainImageContexts.emplace_back(
        SwapchainImageContext::create(GetSwapchainImageType()));
    auto swapchainImageContext = m_swapchainImageContexts.back();

    std::vector<XrSwapchainImageBaseHeader *> bases =
        swapchainImageContext->Create(m_vkDevice, m_memAllocator, capacity,
                                      swapchainCreateInfo, m_pipelineLayout,
                                      m_shaderProgram, m_drawBuffer);

    // Map every swapchainImage base pointer to this context
    for (auto &base : bases) {
      m_swapchainImageContextMap[base] = swapchainImageContext;
    }

    return bases;
  }

  void RenderView(const XrCompositionLayerProjectionView &layerView,
                  const XrSwapchainImageBaseHeader *swapchainImage,
                  int64_t /*swapchainFormat*/,
                  const std::vector<Cube> &cubes) override {
    CHECK(layerView.subImage.imageArrayIndex ==
          0); // Texture arrays not supported.

    auto swapchainContext = m_swapchainImageContextMap[swapchainImage];
    uint32_t imageIndex = swapchainContext->ImageIndex(swapchainImage);

    // XXX Should double-buffer the command buffers, for now just flush
    m_cmdBuffer.Wait();
    m_cmdBuffer.Reset();
    m_cmdBuffer.Begin();

    // Ensure depth is in the right layout
    swapchainContext->depthBuffer->TransitionLayout(
        &m_cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

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

    vkCmdBeginRenderPass(m_cmdBuffer.buf, &renderPassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(m_cmdBuffer.buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      swapchainContext->pipe->pipe);

    // Bind index and vertex buffers
    vkCmdBindIndexBuffer(m_cmdBuffer.buf, m_drawBuffer->idxBuf, 0,
                         VK_INDEX_TYPE_UINT16);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(m_cmdBuffer.buf, 0, 1, &m_drawBuffer->vtxBuf,
                           &offset);

    // Compute the view-projection transform.
    // Note all matrixes (including OpenXR's) are column-major, right-handed.
    const auto &pose = layerView.pose;
    XrMatrix4x4f proj;
    XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_VULKAN, layerView.fov,
                                     0.05f, 100.0f);
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
          &model, &cube.Pose.position, &cube.Pose.orientation, &cube.Scale);
      XrMatrix4x4f mvp;
      XrMatrix4x4f_Multiply(&mvp, &vp, &model);
      vkCmdPushConstants(m_cmdBuffer.buf, m_pipelineLayout.layout,
                         VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvp.m),
                         &mvp.m[0]);

      // Draw the cube.
      vkCmdDrawIndexed(m_cmdBuffer.buf, m_drawBuffer->count.idx, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(m_cmdBuffer.buf);

    m_cmdBuffer.End();
    m_cmdBuffer.Exec(m_vkQueue);

#if defined(USE_MIRROR_WINDOW)
    // Cycle the window's swapchain on the last view rendered
    if (swapchainContext == &m_swapchainImageContexts.back()) {
      m_swapchain.Acquire();
      m_swapchain.Wait();
      m_swapchain.Present(m_vkQueue);
    }
#endif
  }

  uint32_t
  GetSupportedSwapchainSampleCount(const XrViewConfigurationView &) override {
    return VK_SAMPLE_COUNT_1_BIT;
  }

  void UpdateOptions(const std::shared_ptr<Options> &options) override {
    m_clearColor = options->GetBackgroundClearColor();
  }

protected:
  XrGraphicsBindingVulkan2KHR m_graphicsBinding{
      XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
  std::list<std::shared_ptr<SwapchainImageContext>> m_swapchainImageContexts;
  std::map<const XrSwapchainImageBaseHeader *,
           std::shared_ptr<SwapchainImageContext>>
      m_swapchainImageContextMap;

  VkInstance m_vkInstance{VK_NULL_HANDLE};
  VkPhysicalDevice m_vkPhysicalDevice{VK_NULL_HANDLE};
  VkDevice m_vkDevice{VK_NULL_HANDLE};
  uint32_t m_queueFamilyIndex = 0;
  VkQueue m_vkQueue{VK_NULL_HANDLE};
  VkSemaphore m_vkDrawDone{VK_NULL_HANDLE};

  std::shared_ptr<MemoryAllocator> m_memAllocator;
  ShaderProgram m_shaderProgram{};
  CmdBuffer m_cmdBuffer;
  PipelineLayout m_pipelineLayout{};
  std::shared_ptr<VertexBuffer> m_drawBuffer;
  std::array<float, 4> m_clearColor;

#if defined(USE_MIRROR_WINDOW)
  Swapchain m_swapchain{};
#endif

  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT{nullptr};
  VkDebugUtilsMessengerEXT m_vkDebugUtilsMessenger{VK_NULL_HANDLE};

  VkBool32
  debugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
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
    if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) !=
        0u) {
      flagNames += "INFO:";
      level = Log::Level::Info;
    }
    if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) !=
        0u) {
      flagNames += "WARN:";
      level = Log::Level::Warning;
    }
    if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) !=
        0u) {
      flagNames += "ERROR:";
      level = Log::Level::Error;
    }
    if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) !=
        0u) {
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

    Log::Write(level, Fmt("%s (%s 0x%llx) %s", flagNames.c_str(),
                          objName.c_str(), object, pCallbackData->pMessage));

    return VK_FALSE;
  }

  static VKAPI_ATTR VkBool32 VKAPI_CALL
  debugMessageThunk(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                    void *pUserData) {
    return static_cast<VulkanGraphicsPlugin *>(pUserData)->debugMessage(
        messageSeverity, messageTypes, pCallbackData);
  }

  virtual XrStructureType GetGraphicsBindingType() const {
    return XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR;
  }
  virtual XrStructureType GetSwapchainImageType() const {
    return XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR;
  }

  virtual XrResult
  CreateVulkanInstanceKHR(XrInstance instance,
                          const XrVulkanInstanceCreateInfoKHR *createInfo,
                          VkInstance *vulkanInstance, VkResult *vulkanResult) {
    PFN_xrCreateVulkanInstanceKHR pfnCreateVulkanInstanceKHR = nullptr;
    CHECK_XRCMD(xrGetInstanceProcAddr(
        instance, "xrCreateVulkanInstanceKHR",
        reinterpret_cast<PFN_xrVoidFunction *>(&pfnCreateVulkanInstanceKHR)));

    return pfnCreateVulkanInstanceKHR(instance, createInfo, vulkanInstance,
                                      vulkanResult);
  }

  virtual XrResult
  CreateVulkanDeviceKHR(XrInstance instance,
                        const XrVulkanDeviceCreateInfoKHR *createInfo,
                        VkDevice *vulkanDevice, VkResult *vulkanResult) {
    PFN_xrCreateVulkanDeviceKHR pfnCreateVulkanDeviceKHR = nullptr;
    CHECK_XRCMD(xrGetInstanceProcAddr(
        instance, "xrCreateVulkanDeviceKHR",
        reinterpret_cast<PFN_xrVoidFunction *>(&pfnCreateVulkanDeviceKHR)));

    return pfnCreateVulkanDeviceKHR(instance, createInfo, vulkanDevice,
                                    vulkanResult);
  }

  virtual XrResult
  GetVulkanGraphicsDevice2KHR(XrInstance instance,
                              const XrVulkanGraphicsDeviceGetInfoKHR *getInfo,
                              VkPhysicalDevice *vulkanPhysicalDevice) {
    PFN_xrGetVulkanGraphicsDevice2KHR pfnGetVulkanGraphicsDevice2KHR = nullptr;
    CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDevice2KHR",
                                      reinterpret_cast<PFN_xrVoidFunction *>(
                                          &pfnGetVulkanGraphicsDevice2KHR)));

    return pfnGetVulkanGraphicsDevice2KHR(instance, getInfo,
                                          vulkanPhysicalDevice);
  }

  virtual XrResult GetVulkanGraphicsRequirements2KHR(
      XrInstance instance, XrSystemId systemId,
      XrGraphicsRequirementsVulkan2KHR *graphicsRequirements) {
    PFN_xrGetVulkanGraphicsRequirements2KHR
        pfnGetVulkanGraphicsRequirements2KHR = nullptr;
    CHECK_XRCMD(
        xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirements2KHR",
                              reinterpret_cast<PFN_xrVoidFunction *>(
                                  &pfnGetVulkanGraphicsRequirements2KHR)));

    return pfnGetVulkanGraphicsRequirements2KHR(instance, systemId,
                                                graphicsRequirements);
  }
};

std::shared_ptr<IGraphicsPlugin>
CreateGraphicsPlugin_Vulkan(const std::shared_ptr<Options> &options,
                            std::shared_ptr<IPlatformPlugin> platformPlugin) {
  return std::make_shared<VulkanGraphicsPlugin>(options,
                                                std::move(platformPlugin));
}

#endif // XR_USE_GRAPHICS_API_VULKAN

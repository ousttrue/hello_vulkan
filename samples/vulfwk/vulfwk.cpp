#include "vulfwk.h"
#include "logger.h"
#include "vulfwk_swapchain.h"

#include <set>
#include <string>

static bool
checkValidationLayerSupport(const std::vector<const char *> &validationLayers) {
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const char *layerName : validationLayers) {
    bool layerFound = false;

    for (const auto &layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        layerFound = true;
        break;
      }
    }

    if (!layerFound) {
      return false;
    }
  }

  return true;
}

static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {
  LOGE("validation layer: %s", pCallbackData->pMessage);
  return VK_FALSE;
}

static void
DestroyDebugUtilsMessengerEXT(VkInstance instance,
                              VkDebugUtilsMessengerEXT debugMessenger,
                              const VkAllocationCallbacks *pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

static bool
checkDeviceExtensionSupport(VkPhysicalDevice device,
                            const std::vector<const char *> &deviceExtensions) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       availableExtensions.data());

  std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                           deviceExtensions.end());

  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

static bool
isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface,
                 const std::vector<const char *> &deviceExtensions) {
  auto indices = QueueFamilyIndices ::findQueueFamilies(device, surface);
  bool extensionsSupported =
      checkDeviceExtensionSupport(device, deviceExtensions);

  bool swapChainAdequate = false;
  if (extensionsSupported) {
    SwapChainSupportDetails swapChainSupport;
    if (!swapChainSupport.querySwapChainSupport(device, surface)) {
      return false;
    }
    swapChainAdequate = !swapChainSupport.formats.empty() &&
                        !swapChainSupport.presentModes.empty();
  }

  return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

static VkShaderModule createShaderModule(VkDevice device,
                                         const std::vector<char> &code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    LOGE("failed to create shader module!");
    return VK_NULL_HANDLE;
  }

  return shaderModule;
}

VulkanFramework::VulkanFramework(const char *appName, const char *engineName) {
  _appName = appName;
  _engineName = engineName;
  LOGI("*** VulkanFramewor::VulkanFramework ***");
}

VulkanFramework::~VulkanFramework() {
  LOGI("*** VulkanFramewor::~VulkanFramework ***");
  vkDeviceWaitIdle(Device);
  cleanup();
}

bool VulkanFramework::initializeInstance(
    const std::vector<const char *> &layerNames,
    const std::vector<const char *> &extensionNames) {
  for (auto name : layerNames) {
    LOGI("[layer] %s", name);
  }
  if (!checkValidationLayerSupport(layerNames)) {
    LOGE("validation layers requested, but not available!");
    return false;
  }
  for (auto name : extensionNames) {
    LOGI("[extension] %s", name);
  }
  if (!createInstance(layerNames, extensionNames)) {
    return false;
  }
  return true;
}

void VulkanFramework::cleanup() {
  Swapchain = nullptr;

  if (CommandPool) {
    vkDestroyCommandPool(Device, CommandPool, nullptr);
  }
  if (GraphicsPipeline) {
    vkDestroyPipeline(Device, GraphicsPipeline, nullptr);
  }
  if (PipelineLayout) {
    vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
  }
  if (RenderPass) {
    vkDestroyRenderPass(Device, RenderPass, nullptr);
  }
  if (Device) {
    vkDestroyDevice(Device, nullptr);
  }
  if (Surface) {
    vkDestroySurfaceKHR(Instance, Surface, nullptr);
  }
  if (DebugUtilsMessengerEXT) {
    DestroyDebugUtilsMessengerEXT(Instance, DebugUtilsMessengerEXT, nullptr);
  }
  if (Instance != VK_NULL_HANDLE) {
    vkDestroyInstance(Instance, nullptr);
  }
}

#ifdef ANDROID
#include <android_native_app_glue.h>
#define VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(instance, name, pfn)        \
  vulkanSymbolWrapperLoadInstanceSymbol(instance, name,                        \
                                        (PFN_vkVoidFunction *)&(pfn))

bool VulkanFramework::createSurfaceAndroid(void *p) {
  auto pNativeWindow = reinterpret_cast<ANativeWindow *>(p);

  VkAndroidSurfaceCreateInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
      .window = pNativeWindow,
  };

  if (vkCreateAndroidSurfaceKHR(Instance, &info, nullptr, &Surface) !=
      VK_SUCCESS) {
    LOGE("failed: vkCreateAndroidSurfaceKHR");
    return false;
  }

  return true;
}
#else
bool VulkanFramework::createSurfaceWin32(void *hInstance, void *hWnd) {
  VkWin32SurfaceCreateInfoKHR createInfo{
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .flags = 0,
      .hinstance = reinterpret_cast<HINSTANCE>(hInstance),
      .hwnd = reinterpret_cast<HWND>(hWnd),
  };
  if (vkCreateWin32SurfaceKHR(Instance, &createInfo, nullptr, &Surface) !=
      VK_SUCCESS) {
    LOGE("failed to create window surface!");
    return false;
  }
  return true;
}
#endif

bool VulkanFramework::initializeDevice(
    const std::vector<const char *> &layerNames,
    const std::vector<const char *> &deviceExtensionNames) {
  if (!pickPhysicalDevice(deviceExtensionNames)) {
    LOGE("failed: pickPhysicalDevice");
    return false;
  }
  if (!createLogicalDevice(layerNames, deviceExtensionNames)) {
    LOGE("failed: createLogicalDevice");
    return false;
  }

  SwapChainSupportDetails swapChainSupport;
  if (!swapChainSupport.querySwapChainSupport(PhysicalDevice, Surface)) {
    LOGE("failed: querySwapChainSupport");
    return false;
  }

  VkSurfaceFormatKHR surfaceFormat =
      SwapchainImpl::chooseSwapSurfaceFormat(swapChainSupport.formats);
  SwapchainImageFormat = surfaceFormat.format;

  // pipeline
  if (!createRenderPass()) {
    LOGE("failed: createRenderPass");
    return false;
  }
  if (!createGraphicsPipeline()) {
    LOGE("failed: createGraphicsPipeline");
    return false;
  }
  if (!createCommandPool()) {
    LOGE("failed: createCommandPool");
    return false;
  }
  if (!createCommandBuffers()) {
    LOGE("failed: createCommandBuffers");
    return false;
  }
  return true;
}

bool VulkanFramework::createSwapChain(VkExtent2D imageExtent) {
  if (Swapchain) {
    auto swapchainExtent = Swapchain->extent();
    if (swapchainExtent.width == imageExtent.width &&
        swapchainExtent.height == imageExtent.height) {
      return true;
    } else {
      // clear recreate
      vkDeviceWaitIdle(Device);
      Swapchain = nullptr;
    }
  }

  Swapchain =
      SwapchainImpl::create(PhysicalDevice, Surface, Device, imageExtent,
                            SwapchainImageFormat, RenderPass);
  if (!Swapchain) {
    return false;
  }
  return true;
}

bool VulkanFramework::createInstance(
    const std::vector<const char *> &layerNames,
    const std::vector<const char *> &extensionNames) {
  VkDebugUtilsMessengerCreateInfoEXT DebugUtilsMessengerCreateInfoEXT{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debugCallback,
  };

  VkApplicationInfo ApplicationInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = _appName.c_str(),
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = _engineName.c_str(),
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };

  VkInstanceCreateInfo InstanceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = layerNames.size() ? &DebugUtilsMessengerCreateInfoEXT : nullptr,
      .pApplicationInfo = &ApplicationInfo,
      .enabledLayerCount = static_cast<uint32_t>(layerNames.size()),
      .ppEnabledLayerNames = layerNames.data(),
      .enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
      .ppEnabledExtensionNames = extensionNames.data(),
  };

  if (layerNames.size()) {
    if (vkCreateInstance(&InstanceCreateInfo, nullptr, &Instance) !=
        VK_SUCCESS) {
      LOGE("failed to create instance!");
      return false;
    }
  }

  if (CreateDebugUtilsMessengerEXT(Instance, &DebugUtilsMessengerCreateInfoEXT,
                                   nullptr,
                                   &DebugUtilsMessengerEXT) != VK_SUCCESS) {
    LOGE("failed to set up debug messenger!");
    return false;
  }

  return true;
}

bool VulkanFramework::pickPhysicalDevice(
    const std::vector<const char *> &deviceExtensionNames) {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(Instance, &deviceCount, nullptr);
  if (deviceCount == 0) {
    LOGE("failed to find GPUs with Vulkan support!");
    return false;
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(Instance, &deviceCount, devices.data());

  for (const auto &device : devices) {
    if (isDeviceSuitable(device, Surface, deviceExtensionNames)) {
      PhysicalDevice = device;
      break;
    }
  }

  if (PhysicalDevice == VK_NULL_HANDLE) {
    LOGE("failed to find a suitable GPU!");
    return false;
  }

  return true;
}

bool VulkanFramework::createLogicalDevice(
    const std::vector<const char *> &layerNames,
    const std::vector<const char *> &deviceExtensionNames) {
  auto indices =
      QueueFamilyIndices ::findQueueFamilies(PhysicalDevice, Surface);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                            indices.presentFamily.value()};

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};

  VkDeviceCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .enabledLayerCount = static_cast<uint32_t>(layerNames.size()),
      .ppEnabledLayerNames = layerNames.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(deviceExtensionNames.size()),
      .ppEnabledExtensionNames = deviceExtensionNames.data(),
      .pEnabledFeatures = &deviceFeatures,
  };

  if (vkCreateDevice(PhysicalDevice, &createInfo, nullptr, &Device) !=
      VK_SUCCESS) {
    LOGE("failed to create logical device!");
    return false;
  }

  vkGetDeviceQueue(Device, indices.graphicsFamily.value(), 0, &GraphicsQueue);
  vkGetDeviceQueue(Device, indices.presentFamily.value(), 0, &PresentQueue);
  return true;
}

bool VulkanFramework::createRenderPass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = SwapchainImageFormat;
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

  if (vkCreateRenderPass(Device, &renderPassInfo, nullptr, &RenderPass) !=
      VK_SUCCESS) {
    LOGE("failed to create render pass!");
    return false;
  }
  return true;
}

#if ANDROID
static std::vector<char> readFile(const char *filePath, void *p) {
  auto manager = reinterpret_cast<AAssetManager *>(p);
  AAsset *asset = AAssetManager_open(manager, filePath, AASSET_MODE_BUFFER);
  if (!asset) {
    return {};
  }

  size_t size = AAsset_getLength(asset);
  std::vector<char> buffer(size);

  // データを読み込む
  AAsset_read(asset, buffer.data(), size);
  AAsset_close(asset);

  return buffer;
}
#else
#include <fstream>
static std::vector<char> readFile(const char *filename, void *) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    LOGE("failed to open: %s", filename);
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

bool VulkanFramework::createGraphicsPipeline() {
  auto vertShaderCode = readFile("shader.vert.spv", AssetManager);
  if (vertShaderCode.empty()) {
    return false;
  }
  auto fragShaderCode = readFile("shader.frag.spv", AssetManager);
  if (fragShaderCode.empty()) {
    return false;
  }

  VkShaderModule vertShaderModule = createShaderModule(Device, vertShaderCode);
  if (vertShaderModule == VK_NULL_HANDLE) {
    return false;
  }
  VkShaderModule fragShaderModule = createShaderModule(Device, fragShaderCode);
  if (fragShaderModule == VK_NULL_HANDLE) {
    return false;
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

  if (vkCreatePipelineLayout(Device, &pipelineLayoutInfo, nullptr,
                             &PipelineLayout) != VK_SUCCESS) {
    LOGE("failed to create pipeline layout!");
    return false;
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
  pipelineInfo.layout = PipelineLayout;
  pipelineInfo.renderPass = RenderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &GraphicsPipeline) != VK_SUCCESS) {
    LOGE("failed to create graphics pipeline!");
    return false;
  }

  vkDestroyShaderModule(Device, fragShaderModule, nullptr);
  vkDestroyShaderModule(Device, vertShaderModule, nullptr);
  return true;
}

bool VulkanFramework::createCommandPool() {
  auto queueFamilyIndices =
      QueueFamilyIndices ::findQueueFamilies(PhysicalDevice, Surface);

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

  if (vkCreateCommandPool(Device, &poolInfo, nullptr, &CommandPool) !=
      VK_SUCCESS) {
    LOGE("failed to create command pool!");
    return false;
  }
  return true;
}

bool VulkanFramework::createCommandBuffers() {
  CommandBuffers.resize(SwapchainImpl::MAX_FRAMES_IN_FLIGHT);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = CommandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = (uint32_t)CommandBuffers.size();

  if (vkAllocateCommandBuffers(Device, &allocInfo, CommandBuffers.data()) !=
      VK_SUCCESS) {
    LOGE("failed to allocate command buffers!");
    return false;
  }
  return true;
}

bool VulkanFramework::drawFrame(uint32_t width, uint32_t height) {

  createSwapChain({width, height});

  auto [imageIndex, currentFrame] = Swapchain->begin();

  vkResetCommandBuffer(CommandBuffers[currentFrame],
                       /*VkCommandBufferResetFlagBits*/ 0);
  if (!recordCommandBuffer(CommandBuffers[currentFrame], imageIndex,
                           Swapchain->frameBuffer(imageIndex),
                           Swapchain->extent())) {
    return false;
  }

  Swapchain->end(imageIndex, CommandBuffers[currentFrame], GraphicsQueue,
                 PresentQueue);

  return true;
}

bool VulkanFramework::recordCommandBuffer(VkCommandBuffer commandBuffer,
                                          uint32_t imageIndex,
                                          VkFramebuffer framebuffer,
                                          VkExtent2D imageExtent) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    LOGE("failed to begin recording command buffer!");
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
    LOGE("failed to record command buffer!");
    return false;
  }

  return true;
}

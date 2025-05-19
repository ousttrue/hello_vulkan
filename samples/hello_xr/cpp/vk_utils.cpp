#include "vk_utils.h"
#include "MemoryAllocator.h"
#include "RenderPass.h"
#include "check.h"
#include "logger.h"
#include "to_string.h"
#include <common/vulkan_debug_object_namer.hpp>

#ifdef USE_ONLINE_VULKAN_SHADERC
constexpr char VertexShaderGlsl[] =
    R"_(
    #version 430
    #extension GL_ARB_separate_shader_objects : enable

    layout (std140, push_constant) uniform buf
    {
        mat4 mvp;
    } ubuf;

    layout (location = 0) in vec3 Position;
    layout (location = 1) in vec3 Color;

    layout (location = 0) out vec4 oColor;
    out gl_PerVertex
    {
        vec4 gl_Position;
    };

    void main()
    {
        oColor.rgba  = Color.rgba;
        gl_Position = ubuf.mvp * Position;
    }
)_";

constexpr char FragmentShaderGlsl[] =
    R"_(
    #version 430
    #extension GL_ARB_separate_shader_objects : enable

    layout (location = 0) in vec4 oColor;

    layout (location = 0) out vec4 FragColor;

    void main()
    {
        FragColor = oColor;
    }
)_";
#endif // USE_ONLINE_VULKAN_SHADERC

std::string vkResultString(VkResult res) {
  switch (res) {
  case VK_SUCCESS:
    return "SUCCESS";
  case VK_NOT_READY:
    return "NOT_READY";
  case VK_TIMEOUT:
    return "TIMEOUT";
  case VK_EVENT_SET:
    return "EVENT_SET";
  case VK_EVENT_RESET:
    return "EVENT_RESET";
  case VK_INCOMPLETE:
    return "INCOMPLETE";
  case VK_ERROR_OUT_OF_HOST_MEMORY:
    return "ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return "ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED:
    return "ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST:
    return "ERROR_DEVICE_LOST";
  case VK_ERROR_MEMORY_MAP_FAILED:
    return "ERROR_MEMORY_MAP_FAILED";
  case VK_ERROR_LAYER_NOT_PRESENT:
    return "ERROR_LAYER_NOT_PRESENT";
  case VK_ERROR_EXTENSION_NOT_PRESENT:
    return "ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return "ERROR_FEATURE_NOT_PRESENT";
  case VK_ERROR_INCOMPATIBLE_DRIVER:
    return "ERROR_INCOMPATIBLE_DRIVER";
  case VK_ERROR_TOO_MANY_OBJECTS:
    return "ERROR_TOO_MANY_OBJECTS";
  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    return "ERROR_FORMAT_NOT_SUPPORTED";
  case VK_ERROR_SURFACE_LOST_KHR:
    return "ERROR_SURFACE_LOST_KHR";
  case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
    return "ERROR_NATIVE_WINDOW_IN_USE_KHR";
  case VK_SUBOPTIMAL_KHR:
    return "SUBOPTIMAL_KHR";
  case VK_ERROR_OUT_OF_DATE_KHR:
    return "ERROR_OUT_OF_DATE_KHR";
  case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
    return "ERROR_INCOMPATIBLE_DISPLAY_KHR";
  case VK_ERROR_VALIDATION_FAILED_EXT:
    return "ERROR_VALIDATION_FAILED_EXT";
  case VK_ERROR_INVALID_SHADER_NV:
    return "ERROR_INVALID_SHADER_NV";
  default:
    return std::to_string(res);
  }
}

[[noreturn]] void ThrowVkResult(VkResult res, const char *originator,
                                const char *sourceLocation) {
  Throw(Fmt("VkResult failure [%s]", vkResultString(res).c_str()), originator,
        sourceLocation);
}

VkResult CheckVkResult(VkResult res, const char *originator,
                       const char *sourceLocation) {
  if ((res) < VK_SUCCESS) {
    ThrowVkResult(res, originator, sourceLocation);
  }

  return res;
}

// XXX These really shouldn't have trailing ';'s
#define THROW_VK(res, cmd) ThrowVkResult(res, #cmd, FILE_AND_LINE);
#define CHECK_VKCMD(cmd) CheckVkResult(cmd, #cmd, FILE_AND_LINE);
#define CHECK_VKRESULT(res, cmdStr) CheckVkResult(res, cmdStr, FILE_AND_LINE);

//
// CmdBuffer - manage VkCommandBuffer state
//
CmdBuffer::~CmdBuffer() {
  SetState(CmdBufferState::Undefined);
  if (m_vkDevice != nullptr) {
    if (buf != VK_NULL_HANDLE) {
      vkFreeCommandBuffers(m_vkDevice, pool, 1, &buf);
    }
    if (pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(m_vkDevice, pool, nullptr);
    }
    if (execFence != VK_NULL_HANDLE) {
      vkDestroyFence(m_vkDevice, execFence, nullptr);
    }
  }
  buf = VK_NULL_HANDLE;
  pool = VK_NULL_HANDLE;
  execFence = VK_NULL_HANDLE;
  m_vkDevice = nullptr;
}

std::string CmdBuffer::StateString(CmdBufferState s) {
  switch (s) {
#define MK_CASE(name)                                                          \
  case CmdBufferState::name:                                                   \
    return #name;
    LIST_CMDBUFFER_STATES(MK_CASE)
#undef MK_CASE
  }
  return "(Unknown)";
}

#define CHECK_CBSTATE(s)                                                       \
  do                                                                           \
    if (state != (s)) {                                                        \
      Log::Write(Log::Level::Error,                                            \
                 std::string("Expecting state " #s " from ") + __FUNCTION__ +  \
                     ", in " + StateString(state));                            \
      return false;                                                            \
    }                                                                          \
  while (0)

bool CmdBuffer::Init(const VulkanDebugObjectNamer &namer, VkDevice device,
                     uint32_t queueFamilyIndex) {
  CHECK_CBSTATE(CmdBufferState::Undefined);

  m_vkDevice = device;

  // Create a command pool to allocate our command buffer from
  VkCommandPoolCreateInfo cmdPoolInfo{
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
  CHECK_VKCMD(vkCreateCommandPool(m_vkDevice, &cmdPoolInfo, nullptr, &pool));
  CHECK_VKCMD(namer.SetName(VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)pool,
                            "hello_xr command pool"));

  // Create the command buffer from the command pool
  VkCommandBufferAllocateInfo cmd{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cmd.commandPool = pool;
  cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd.commandBufferCount = 1;
  CHECK_VKCMD(vkAllocateCommandBuffers(m_vkDevice, &cmd, &buf));
  CHECK_VKCMD(namer.SetName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)buf,
                            "hello_xr command buffer"));

  VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  CHECK_VKCMD(vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &execFence));
  CHECK_VKCMD(namer.SetName(VK_OBJECT_TYPE_FENCE, (uint64_t)execFence,
                            "hello_xr fence"));

  SetState(CmdBufferState::Initialized);
  return true;
}

bool CmdBuffer::Begin() {
  CHECK_CBSTATE(CmdBufferState::Initialized);
  VkCommandBufferBeginInfo cmdBeginInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  CHECK_VKCMD(vkBeginCommandBuffer(buf, &cmdBeginInfo));
  SetState(CmdBufferState::Recording);
  return true;
}

bool CmdBuffer::End() {
  CHECK_CBSTATE(CmdBufferState::Recording);
  CHECK_VKCMD(vkEndCommandBuffer(buf));
  SetState(CmdBufferState::Executable);
  return true;
}

bool CmdBuffer::Exec(VkQueue queue) {
  CHECK_CBSTATE(CmdBufferState::Executable);

  VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &buf;
  CHECK_VKCMD(vkQueueSubmit(queue, 1, &submitInfo, execFence));

  SetState(CmdBufferState::Executing);
  return true;
}

bool CmdBuffer::Wait() {
  // Waiting on a not-in-flight command buffer is a no-op
  if (state == CmdBufferState::Initialized) {
    return true;
  }

  CHECK_CBSTATE(CmdBufferState::Executing);

  const uint32_t timeoutNs = 1 * 1000 * 1000 * 1000;
  for (int i = 0; i < 5; ++i) {
    auto res = vkWaitForFences(m_vkDevice, 1, &execFence, VK_TRUE, timeoutNs);
    if (res == VK_SUCCESS) {
      // Buffer can be executed multiple times...
      SetState(CmdBufferState::Executable);
      return true;
    }
    Log::Write(Log::Level::Info,
               "Waiting for CmdBuffer fence timed out, retrying...");
  }

  return false;
}

bool CmdBuffer::Reset() {
  if (state != CmdBufferState::Initialized) {
    CHECK_CBSTATE(CmdBufferState::Executable);

    CHECK_VKCMD(vkResetFences(m_vkDevice, 1, &execFence));
    CHECK_VKCMD(vkResetCommandBuffer(buf, 0));

    SetState(CmdBufferState::Initialized);
  }

  return true;
}

//
// VkImage + framebuffer wrapper
//
RenderTarget::~RenderTarget() {
  if (m_vkDevice != nullptr) {
    if (fb != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(m_vkDevice, fb, nullptr);
    }
    if (colorView != VK_NULL_HANDLE) {
      vkDestroyImageView(m_vkDevice, colorView, nullptr);
    }
    if (depthView != VK_NULL_HANDLE) {
      vkDestroyImageView(m_vkDevice, depthView, nullptr);
    }
  }

  // Note we don't own color/depthImage, it will get destroyed when
  // xrDestroySwapchain is called
  colorImage = VK_NULL_HANDLE;
  depthImage = VK_NULL_HANDLE;
  colorView = VK_NULL_HANDLE;
  depthView = VK_NULL_HANDLE;
  fb = VK_NULL_HANDLE;
  m_vkDevice = nullptr;
}

RenderTarget::RenderTarget(RenderTarget &&other) noexcept : RenderTarget() {
  using std::swap;
  swap(colorImage, other.colorImage);
  swap(depthImage, other.depthImage);
  swap(colorView, other.colorView);
  swap(depthView, other.depthView);
  swap(fb, other.fb);
  swap(m_vkDevice, other.m_vkDevice);
}

RenderTarget &RenderTarget::operator=(RenderTarget &&other) noexcept {
  if (&other == this) {
    return *this;
  }
  // Clean up ourselves.
  this->~RenderTarget();
  using std::swap;
  swap(colorImage, other.colorImage);
  swap(depthImage, other.depthImage);
  swap(colorView, other.colorView);
  swap(depthView, other.depthView);
  swap(fb, other.fb);
  swap(m_vkDevice, other.m_vkDevice);
  return *this;
}
void RenderTarget::Create(const VulkanDebugObjectNamer &namer, VkDevice device,
                          VkImage aColorImage, VkImage aDepthImage,
                          VkExtent2D size, RenderPass &renderPass) {
  m_vkDevice = device;

  colorImage = aColorImage;
  depthImage = aDepthImage;

  std::array<VkImageView, 2> attachments{};
  uint32_t attachmentCount = 0;

  // Create color image view
  if (colorImage != VK_NULL_HANDLE) {
    VkImageViewCreateInfo colorViewInfo{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    colorViewInfo.image = colorImage;
    colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorViewInfo.format = renderPass.colorFmt;
    colorViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    colorViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    colorViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    colorViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorViewInfo.subresourceRange.baseMipLevel = 0;
    colorViewInfo.subresourceRange.levelCount = 1;
    colorViewInfo.subresourceRange.baseArrayLayer = 0;
    colorViewInfo.subresourceRange.layerCount = 1;
    CHECK_VKCMD(
        vkCreateImageView(m_vkDevice, &colorViewInfo, nullptr, &colorView));
    CHECK_VKCMD(namer.SetName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)colorView,
                              "hello_xr color image view"));
    attachments[attachmentCount++] = colorView;
  }

  // Create depth image view
  if (depthImage != VK_NULL_HANDLE) {
    VkImageViewCreateInfo depthViewInfo{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    depthViewInfo.image = depthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = renderPass.depthFmt;
    depthViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    depthViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    depthViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    depthViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;
    CHECK_VKCMD(
        vkCreateImageView(m_vkDevice, &depthViewInfo, nullptr, &depthView));
    CHECK_VKCMD(namer.SetName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)depthView,
                              "hello_xr depth image view"));
    attachments[attachmentCount++] = depthView;
  }

  VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  fbInfo.renderPass = renderPass.pass;
  fbInfo.attachmentCount = attachmentCount;
  fbInfo.pAttachments = attachments.data();
  fbInfo.width = size.width;
  fbInfo.height = size.height;
  fbInfo.layers = 1;
  CHECK_VKCMD(vkCreateFramebuffer(m_vkDevice, &fbInfo, nullptr, &fb));
  CHECK_VKCMD(namer.SetName(VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)fb,
                            "hello_xr framebuffer"));
}

//
// DepthBuffer
//
DepthBuffer::~DepthBuffer() {
  if (m_vkDevice != nullptr) {
    if (depthImage != VK_NULL_HANDLE) {
      vkDestroyImage(m_vkDevice, depthImage, nullptr);
    }
    if (depthMemory != VK_NULL_HANDLE) {
      vkFreeMemory(m_vkDevice, depthMemory, nullptr);
    }
  }
  depthImage = VK_NULL_HANDLE;
  depthMemory = VK_NULL_HANDLE;
  m_vkDevice = nullptr;
}

DepthBuffer::DepthBuffer(DepthBuffer &&other) noexcept : DepthBuffer() {
  using std::swap;

  swap(depthImage, other.depthImage);
  swap(depthMemory, other.depthMemory);
  swap(m_vkDevice, other.m_vkDevice);
}
DepthBuffer &DepthBuffer::operator=(DepthBuffer &&other) noexcept {
  if (&other == this) {
    return *this;
  }
  // clean up self
  this->~DepthBuffer();
  using std::swap;

  swap(depthImage, other.depthImage);
  swap(depthMemory, other.depthMemory);
  swap(m_vkDevice, other.m_vkDevice);
  return *this;
}

void DepthBuffer::Create(const VulkanDebugObjectNamer &namer, VkDevice device,
                         MemoryAllocator *memAllocator, VkFormat depthFormat,
                         const XrSwapchainCreateInfo &swapchainCreateInfo) {
  m_vkDevice = device;

  VkExtent2D size = {swapchainCreateInfo.width, swapchainCreateInfo.height};

  // Create a D32 depthbuffer
  VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = size.width;
  imageInfo.extent.height = size.height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = depthFormat;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  imageInfo.samples = (VkSampleCountFlagBits)swapchainCreateInfo.sampleCount;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  CHECK_VKCMD(vkCreateImage(device, &imageInfo, nullptr, &depthImage));
  CHECK_VKCMD(namer.SetName(VK_OBJECT_TYPE_IMAGE, (uint64_t)depthImage,
                            "hello_xr fallback depth image"));

  VkMemoryRequirements memRequirements{};
  vkGetImageMemoryRequirements(device, depthImage, &memRequirements);
  memAllocator->Allocate(memRequirements, &depthMemory,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  CHECK_VKCMD(namer.SetName(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)depthMemory,
                            "hello_xr fallback depth image memory"));
  CHECK_VKCMD(vkBindImageMemory(device, depthImage, depthMemory, 0));
}

void DepthBuffer::TransitionLayout(CmdBuffer *cmdBuffer,
                                   VkImageLayout newLayout) {
  if (newLayout == m_vkLayout) {
    return;
  }

  VkImageMemoryBarrier depthBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  depthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  depthBarrier.oldLayout = m_vkLayout;
  depthBarrier.newLayout = newLayout;
  depthBarrier.image = depthImage;
  depthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
  vkCmdPipelineBarrier(cmdBuffer->buf, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                       VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &depthBarrier);

  m_vkLayout = newLayout;
}

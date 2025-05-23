#include "Swapchain.h"
#include "SwapchainImageContext.h"
#include "VulkanGraphicsPlugin.h"
#include "check.h"
#include "logger.h"
#include "openxr/openxr.h"
#include "to_string.h"

Swapchain::~Swapchain() { xrDestroySwapchain(m_swapchain); }

std::shared_ptr<Swapchain> Swapchain::Create(XrSession session, uint32_t i,
                                             const XrViewConfigurationView &vp,
                                             int64_t format) {

  Log::Write(Log::Level::Info,
             Fmt("Creating swapchain for view %d with dimensions "
                 "Width=%d Height=%d SampleCount=%d",
                 i, vp.recommendedImageRectWidth, vp.recommendedImageRectHeight,
                 vp.recommendedSwapchainSampleCount));

  // Create the swapchain.
  auto ptr = std::shared_ptr<Swapchain>(new Swapchain);
  ptr->m_swapchainCreateInfo = {
      .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
      .createFlags = 0,
      .usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
      .format = format,
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .width = vp.recommendedImageRectWidth,
      .height = vp.recommendedImageRectHeight,
      .faceCount = 1,
      .arraySize = 1,
      .mipCount = 1,
  };
  CHECK_XRCMD(xrCreateSwapchain(session, &ptr->m_swapchainCreateInfo,
                                &ptr->m_swapchain));
  return ptr;
}

void Swapchain::AllocateSwapchainImageStructs(
    const std::shared_ptr<class VulkanGraphicsPlugin> &vulkan) {

  uint32_t imageCount;
  CHECK_XRCMD(xrEnumerateSwapchainImages(m_swapchain, 0, &imageCount, nullptr));
  m_swapchainImages.resize(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});

  std::vector<XrSwapchainImageBaseHeader *> pointers;
  for (auto &image : m_swapchainImages) {
    pointers.push_back((XrSwapchainImageBaseHeader *)&image);
  }
  CHECK_XRCMD(xrEnumerateSwapchainImages(m_swapchain, imageCount, &imageCount,
                                         pointers[0]));

  m_context = SwapchainImageContext::Create(
      vulkan->m_vkDevice, vulkan->m_memAllocator, imageCount,
      {m_swapchainCreateInfo.width, m_swapchainCreateInfo.height},
      static_cast<VkFormat>(m_swapchainCreateInfo.format),
      static_cast<VkSampleCountFlagBits>(m_swapchainCreateInfo.sampleCount),
      vulkan->m_shaderProgram);
}

ViewSwapchainInfo Swapchain::AcquireSwapchainForView(const XrView &view) {
  XrSwapchainImageAcquireInfo acquireInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
  };
  uint32_t swapchainImageIndex;
  CHECK_XRCMD(
      xrAcquireSwapchainImage(m_swapchain, &acquireInfo, &swapchainImageIndex));

  XrSwapchainImageWaitInfo waitInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
      .timeout = XR_INFINITE_DURATION,
  };
  CHECK_XRCMD(xrWaitSwapchainImage(m_swapchain, &waitInfo));

  ViewSwapchainInfo info = {
      .Swapchain = m_context,
      .Image = m_swapchainImages[swapchainImageIndex].image,
      .CompositionLayer = {
          .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
          .pose = view.pose,
          .fov = view.fov,
          .subImage =
              {
                  .swapchain = m_swapchain,
                  .imageRect =
                      {
                          .offset = {0, 0},
                          .extent = {.width = static_cast<int32_t>(
                                         m_swapchainCreateInfo.width),
                                     .height = static_cast<int32_t>(
                                         m_swapchainCreateInfo.height)},
                      },
              },
      }};

  return info;
}

void Swapchain::EndSwapchain() {
  XrSwapchainImageReleaseInfo releaseInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
  };
  CHECK_XRCMD(xrReleaseSwapchainImage(m_swapchain, &releaseInfo));
}

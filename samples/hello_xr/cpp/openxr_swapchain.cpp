#include "openxr_swapchain.h"
#include <xro/xro.h>

struct SwapchainImpl {
  std::vector<XrSwapchainImageVulkan2KHR> m_swapchainImages;
};

OpenXrSwapchain::OpenXrSwapchain() : m_impl(new SwapchainImpl) {}

OpenXrSwapchain::~OpenXrSwapchain() {
  xrDestroySwapchain(m_swapchain);
  delete m_impl;
}

std::shared_ptr<OpenXrSwapchain>
OpenXrSwapchain::Create(XrSession session, uint32_t i,
                        const XrViewConfigurationView &vp, int64_t format) {

  xro::Logger::Info("Creating swapchain for view %d with dimensions "
                    "Width=%d Height=%d SampleCount=%d",
                    i, vp.recommendedImageRectWidth,
                    vp.recommendedImageRectHeight,
                    vp.recommendedSwapchainSampleCount);

  // Create the swapchain.
  auto ptr = std::shared_ptr<OpenXrSwapchain>(new OpenXrSwapchain);
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
  XRO_CHECK(xrCreateSwapchain(session, &ptr->m_swapchainCreateInfo,
                              &ptr->m_swapchain));

  uint32_t imageCount;
  XRO_CHECK(
      xrEnumerateSwapchainImages(ptr->m_swapchain, 0, &imageCount, nullptr));
  ptr->m_impl->m_swapchainImages.resize(imageCount,
                                        {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});

  std::vector<XrSwapchainImageBaseHeader *> pointers;
  for (auto &image : ptr->m_impl->m_swapchainImages) {
    pointers.push_back((XrSwapchainImageBaseHeader *)&image);
  }
  XRO_CHECK(xrEnumerateSwapchainImages(ptr->m_swapchain, imageCount,
                                       &imageCount, pointers[0]));

  return ptr;
}

ViewSwapchainInfo OpenXrSwapchain::AcquireSwapchain(const XrView &view) {
  XrSwapchainImageAcquireInfo acquireInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
  };
  uint32_t swapchainImageIndex;
  XRO_CHECK(
      xrAcquireSwapchainImage(m_swapchain, &acquireInfo, &swapchainImageIndex));

  XrSwapchainImageWaitInfo waitInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
      .timeout = XR_INFINITE_DURATION,
  };
  XRO_CHECK(xrWaitSwapchainImage(m_swapchain, &waitInfo));

  ViewSwapchainInfo info = {
      .Image = m_impl->m_swapchainImages[swapchainImageIndex].image,
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

void OpenXrSwapchain::EndSwapchain() {
  XrSwapchainImageReleaseInfo releaseInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
  };
  XRO_CHECK(xrReleaseSwapchainImage(m_swapchain, &releaseInfo));
}

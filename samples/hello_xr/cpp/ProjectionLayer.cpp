#include "ProjectionLayer.h"
#include "SwapchainImageContext.h"
#include "VulkanGraphicsPlugin.h"
#include "check.h"
#include "common/xr_linear.h"
#include "logger.h"
#include "to_string.h"

ProjectionLayer::~ProjectionLayer() {
  for (Swapchain swapchain : m_swapchains) {
    xrDestroySwapchain(swapchain.handle);
  }
}

std::shared_ptr<ProjectionLayer> ProjectionLayer::Create(
    XrInstance instance, XrSystemId systemId, XrSession session,
    XrViewConfigurationType viewConfigurationType,
    const std::shared_ptr<class VulkanGraphicsPlugin> &vulkan) {

  auto ptr = std::shared_ptr<ProjectionLayer>(new ProjectionLayer);

  // Query and cache view configuration views.
  uint32_t viewCount;
  CHECK_XRCMD(xrEnumerateViewConfigurationViews(
      instance, systemId, viewConfigurationType, 0, &viewCount, nullptr));
  ptr->m_configViews.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  CHECK_XRCMD(xrEnumerateViewConfigurationViews(
      instance, systemId, viewConfigurationType, viewCount, &viewCount,
      ptr->m_configViews.data()));

  // Create and cache view buffer for xrLocateViews later.
  ptr->m_views.resize(viewCount, {XR_TYPE_VIEW});

  // Create the swapchain and get the images.
  if (viewCount > 0) {
    // Select a swapchain format.
    uint32_t swapchainFormatCount;
    CHECK_XRCMD(xrEnumerateSwapchainFormats(session, 0, &swapchainFormatCount,
                                            nullptr));
    std::vector<int64_t> swapchainFormats(swapchainFormatCount);
    CHECK_XRCMD(xrEnumerateSwapchainFormats(
        session, (uint32_t)swapchainFormats.size(), &swapchainFormatCount,
        swapchainFormats.data()));
    CHECK(swapchainFormatCount == swapchainFormats.size());
    ptr->m_colorSwapchainFormat =
        vulkan->SelectColorSwapchainFormat(swapchainFormats);

    // Print swapchain formats and the selected one.
    {
      std::string swapchainFormatsString;
      for (int64_t format : swapchainFormats) {
        const bool selected = format == ptr->m_colorSwapchainFormat;
        swapchainFormatsString += " ";
        if (selected) {
          swapchainFormatsString += "[";
        }
        swapchainFormatsString += std::to_string(format);
        if (selected) {
          swapchainFormatsString += "]";
        }
      }
      Log::Write(Log::Level::Verbose,
                 Fmt("Swapchain Formats: %s", swapchainFormatsString.c_str()));
    }

    // Create a swapchain for each view.
    for (uint32_t i = 0; i < viewCount; i++) {
      const XrViewConfigurationView &vp = ptr->m_configViews[i];
      Log::Write(Log::Level::Info,
                 Fmt("Creating swapchain for view %d with dimensions "
                     "Width=%d Height=%d SampleCount=%d",
                     i, vp.recommendedImageRectWidth,
                     vp.recommendedImageRectHeight,
                     vp.recommendedSwapchainSampleCount));

      // Create the swapchain.
      XrSwapchainCreateInfo swapchainCreateInfo{
          .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
          .createFlags = 0,
          .usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
          .format = ptr->m_colorSwapchainFormat,
          .sampleCount = VK_SAMPLE_COUNT_1_BIT,
          .width = vp.recommendedImageRectWidth,
          .height = vp.recommendedImageRectHeight,
          .faceCount = 1,
          .arraySize = 1,
          .mipCount = 1,
      };

      ProjectionLayer::Swapchain swapchain{
          .extent =
              {
                  .width = static_cast<int32_t>(swapchainCreateInfo.width),
                  .height = static_cast<int32_t>(swapchainCreateInfo.height),
              },
      };
      CHECK_XRCMD(
          xrCreateSwapchain(session, &swapchainCreateInfo, &swapchain.handle));
      ptr->m_swapchains.push_back(swapchain);

      uint32_t imageCount;
      CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount,
                                             nullptr));
      // XXX This should really just return XrSwapchainImageBaseHeader*
      std::vector<XrSwapchainImageBaseHeader *> swapchainImages =
          ptr->AllocateSwapchainImageStructs(
              imageCount,
              {swapchainCreateInfo.width, swapchainCreateInfo.height},
              static_cast<VkFormat>(swapchainCreateInfo.format),
              static_cast<VkSampleCountFlagBits>(
                  swapchainCreateInfo.sampleCount),
              vulkan);
      CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, imageCount,
                                             &imageCount, swapchainImages[0]));

      ptr->m_swapchainImages.insert(
          std::make_pair(swapchain.handle, std::move(swapchainImages)));
    }
  }

  return ptr;
}

std::vector<XrSwapchainImageBaseHeader *>
ProjectionLayer::AllocateSwapchainImageStructs(
    uint32_t capacity, VkExtent2D size, VkFormat format,
    VkSampleCountFlagBits sampleCount,
    const std::shared_ptr<class VulkanGraphicsPlugin> &vulkan) {
  // Allocate and initialize the buffer of image structs (must be sequential
  // in memory for xrEnumerateSwapchainImages). Return back an array of
  // pointers to each swapchain image struct so the consumer doesn't need to
  // know the type/size. Keep the buffer alive by adding it into the list of
  // buffers.
  m_swapchainImageContexts.emplace_back(SwapchainImageContext::Create(
      vulkan->m_vkDevice, vulkan->m_memAllocator, capacity, size, format,
      sampleCount, vulkan->m_pipelineLayout, vulkan->m_shaderProgram,
      vulkan->m_drawBuffer));
  auto swapchainImageContext = m_swapchainImageContexts.back();

  // Map every swapchainImage base pointer to this context
  for (auto &base : swapchainImageContext->m_bases) {
    m_swapchainImageContextMap[base] = swapchainImageContext;
  }

  return swapchainImageContext->m_bases;
}

bool ProjectionLayer::LocateView(XrSession session, XrSpace appSpace,
                                 XrTime predictedDisplayTime,
                                 XrViewConfigurationType viewConfigType,
                                 uint32_t *viewCountOutput) {
  XrViewLocateInfo viewLocateInfo{
      .type = XR_TYPE_VIEW_LOCATE_INFO,
      .viewConfigurationType = viewConfigType,
      .displayTime = predictedDisplayTime,
      .space = appSpace,
  };

  XrViewState viewState{
      .type = XR_TYPE_VIEW_STATE,
  };

  auto res = xrLocateViews(session, &viewLocateInfo, &viewState,
                           static_cast<uint32_t>(m_views.size()),
                           viewCountOutput, m_views.data());
  CHECK_XRRESULT(res, "xrLocateViews");
  if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
      (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
    return false; // There is no valid tracking poses for the views.
  }

  CHECK(*viewCountOutput == m_views.size());
  CHECK(*viewCountOutput == m_configViews.size());
  CHECK(*viewCountOutput == m_swapchains.size());

  return true;
}

ProjectionLayer::ViewSwapchainInfo
ProjectionLayer::AcquireSwapchainForView(uint32_t i) {
  // Each view has a separate swapchain which is acquired, rendered to, and
  // released.
  const Swapchain viewSwapchain = m_swapchains[i];

  XrSwapchainImageAcquireInfo acquireInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
  };
  uint32_t swapchainImageIndex;
  CHECK_XRCMD(xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo,
                                      &swapchainImageIndex));

  XrSwapchainImageWaitInfo waitInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
      .timeout = XR_INFINITE_DURATION,
  };
  CHECK_XRCMD(xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo));

  const XrSwapchainImageBaseHeader *const swapchainImage =
      m_swapchainImages[viewSwapchain.handle][swapchainImageIndex];

  ViewSwapchainInfo info = {
  };

  info.CompositionLayer = {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
      .pose = m_views[i].pose,
      .fov = m_views[i].fov,
      .subImage = {
          .swapchain = viewSwapchain.handle,
          .imageRect = {.offset = {0, 0}, .extent = viewSwapchain.extent}}};

  info.Swapchain = m_swapchainImageContextMap[swapchainImage];
  info.ImageIndex = info.Swapchain->ImageIndex(swapchainImage);

   return info;
}

void ProjectionLayer::EndSwapchain(XrSwapchain swapchain) {
  XrSwapchainImageReleaseInfo releaseInfo{
      .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
  };
  CHECK_XRCMD(xrReleaseSwapchainImage(swapchain, &releaseInfo));
}

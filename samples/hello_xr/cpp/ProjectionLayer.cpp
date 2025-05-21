#include "ProjectionLayer.h"
#include "VulkanGraphicsPlugin.h"
#include "check.h"
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
    const std::shared_ptr<struct VulkanGraphicsPlugin> &vulkan) {

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
          vulkan->AllocateSwapchainImageStructs(
              imageCount,
              {swapchainCreateInfo.width, swapchainCreateInfo.height},
              static_cast<VkFormat>(swapchainCreateInfo.format),
              static_cast<VkSampleCountFlagBits>(
                  swapchainCreateInfo.sampleCount));
      CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, imageCount,
                                             &imageCount, swapchainImages[0]));

      ptr->m_swapchainImages.insert(
          std::make_pair(swapchain.handle, std::move(swapchainImages)));
    }
  }

  return ptr;
}

static Cube MakeCube(XrPosef pose, XrVector3f scale) {
  return Cube{
      .Translaton =
          {
              pose.position.x,
              pose.position.y,
              pose.position.z,
          },
      .Rotation =
          {
              pose.orientation.x,
              pose.orientation.y,
              pose.orientation.z,
              pose.orientation.w,
          },
      .Scaling =
          {
              scale.x,
              scale.y,
              scale.z,
          },
  };
}

XrCompositionLayerProjection *ProjectionLayer::RenderLayer(
    XrSession session, XrTime predictedDisplayTime, XrSpace appSpace,
    XrViewConfigurationType viewConfigType,
    const std::vector<XrSpace> &visualizedSpaces, const InputState &input,
    const std::shared_ptr<struct VulkanGraphicsPlugin> &vulkan,
    XrEnvironmentBlendMode environmentBlendMode) {

  // uint32_t viewCapacityInput = (uint32_t);

  XrViewLocateInfo viewLocateInfo{
      .type = XR_TYPE_VIEW_LOCATE_INFO,
      .viewConfigurationType = viewConfigType,
      .displayTime = predictedDisplayTime,
      .space = appSpace,
  };

  XrViewState viewState{
      .type = XR_TYPE_VIEW_STATE,
  };

  uint32_t viewCountOutput;
  auto res = xrLocateViews(session, &viewLocateInfo, &viewState,
                           static_cast<uint32_t>(m_views.size()),
                           &viewCountOutput, m_views.data());
  CHECK_XRRESULT(res, "xrLocateViews");
  if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
      (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
    return {}; // There is no valid tracking poses for the views.
  }

  CHECK(viewCountOutput == m_views.size());
  CHECK(viewCountOutput == m_configViews.size());
  CHECK(viewCountOutput == m_swapchains.size());

  m_projectionLayerViews.resize(viewCountOutput);

  // For each locatable space that we want to visualize, render a 25cm cube.
  std::vector<Cube> cubes;

  for (XrSpace visualizedSpace : visualizedSpaces) {
    XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
    res = xrLocateSpace(visualizedSpace, appSpace, predictedDisplayTime,
                        &spaceLocation);
    CHECK_XRRESULT(res, "xrLocateSpace");
    if (XR_UNQUALIFIED_SUCCESS(res)) {
      if ((spaceLocation.locationFlags &
           XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
          (spaceLocation.locationFlags &
           XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
        cubes.push_back(MakeCube(spaceLocation.pose, {0.25f, 0.25f, 0.25f}));
      }
    } else {
      Log::Write(Log::Level::Verbose, Fmt("Unable to locate a visualized "
                                          "reference space in app space: %d",
                                          res));
    }
  }

  // Render a 10cm cube scaled by grabAction for each hand. Note renderHand
  // will only be true when the application has focus.
  for (auto hand : {Side::LEFT, Side::RIGHT}) {
    XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
    res = xrLocateSpace(input.handSpace[hand], appSpace, predictedDisplayTime,
                        &spaceLocation);
    CHECK_XRRESULT(res, "xrLocateSpace");
    if (XR_UNQUALIFIED_SUCCESS(res)) {
      if ((spaceLocation.locationFlags &
           XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
          (spaceLocation.locationFlags &
           XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
        float scale = 0.1f * input.handScale[hand];
        cubes.push_back(MakeCube(spaceLocation.pose, {scale, scale, scale}));
      }
    } else {
      // Tracking loss is expected when the hand is not active so only log a
      // message if the hand is active.
      if (input.handActive[hand] == XR_TRUE) {
        const char *handName[] = {"left", "right"};
        Log::Write(Log::Level::Verbose,
                   Fmt("Unable to locate %s hand action space in app space: %d",
                       handName[hand], res));
      }
    }
  }

  // Render view to the appropriate part of the swapchain image.
  for (uint32_t i = 0; i < viewCountOutput; i++) {
    // Each view has a separate swapchain which is acquired, rendered to, and
    // released.
    const Swapchain viewSwapchain = m_swapchains[i];

    XrSwapchainImageAcquireInfo acquireInfo{
        XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};

    uint32_t swapchainImageIndex;
    CHECK_XRCMD(xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo,
                                        &swapchainImageIndex));

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    CHECK_XRCMD(xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo));

    m_projectionLayerViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
    m_projectionLayerViews[i].pose = m_views[i].pose;
    m_projectionLayerViews[i].fov = m_views[i].fov;
    m_projectionLayerViews[i].subImage.swapchain = viewSwapchain.handle;
    m_projectionLayerViews[i].subImage.imageRect.offset = {0, 0};
    m_projectionLayerViews[i].subImage.imageRect.extent = viewSwapchain.extent;

    const XrSwapchainImageBaseHeader *const swapchainImage =
        m_swapchainImages[viewSwapchain.handle][swapchainImageIndex];
    vulkan->RenderView(m_projectionLayerViews[i], swapchainImage,
                       m_colorSwapchainFormat, cubes);

    XrSwapchainImageReleaseInfo releaseInfo{
        XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    CHECK_XRCMD(xrReleaseSwapchainImage(viewSwapchain.handle, &releaseInfo));
  }

  m_layer.space = appSpace;
  m_layer.layerFlags =
      environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND
          ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT |
                XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT
          : 0;
  m_layer.viewCount = (uint32_t)m_projectionLayerViews.size();
  m_layer.views = m_projectionLayerViews.data();
  return &m_layer;
}

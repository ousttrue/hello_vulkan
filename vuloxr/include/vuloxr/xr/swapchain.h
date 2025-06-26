#pragma once

#include "../xr.h"
#include <magic_enum/magic_enum.hpp>

namespace vuloxr {

namespace xr {

template <typename T> // XrSwapchainImageVulkan2KHR
struct Swapchain : NonCopyable {
  std::vector<T> swapchainImages;
  XrSwapchainCreateInfo swapchainCreateInfo;
  XrSwapchain swapchain;

  Swapchain(XrSession session, uint32_t i, const XrViewConfigurationView &vp,
            int64_t format, const T &defaultImage) {

    Logger::Info("Creating swapchain for view %d with dimensions "
                 "Width=%d Height=%d SampleCount=%d",
                 i, vp.recommendedImageRectWidth, vp.recommendedImageRectHeight,
                 vp.recommendedSwapchainSampleCount);

    // Create the swapchain.
    this->swapchainCreateInfo = {
        .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
        .createFlags = 0,
        .usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                      XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
        .format = format,
        .sampleCount = 1,
        .width = vp.recommendedImageRectWidth,
        .height = vp.recommendedImageRectHeight,
        .faceCount = 1,
        .arraySize = 1,
        .mipCount = 1,
    };
    CheckXrResult(xrCreateSwapchain(session, &this->swapchainCreateInfo,
                                    &this->swapchain));
    // static XrSwapchain oxr_create_swapchain(XrSession session, uint32_t
    // width,
    //                                         uint32_t height) {
    // XrSwapchainCreateInfo ci = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
    // ci.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
    // XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT; ci.format = GL_RGBA8; ci.width =
    // width; ci.height = height; ci.faceCount = 1; ci.arraySize = 1;
    // ci.mipCount = 1;
    // ci.sampleCount = 1;

    // uint32_t imgCnt;
    // xrEnumerateSwapchainImages(swapchain, 0, &imgCnt, NULL);
    // XrSwapchainImageOpenGLESKHR *img_gles =
    //     (XrSwapchainImageOpenGLESKHR *)calloc(
    //         sizeof(XrSwapchainImageOpenGLESKHR), imgCnt);
    // for (uint32_t i = 0; i < imgCnt; i++)
    //   img_gles[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
    // xrEnumerateSwapchainImages(swapchain, imgCnt, &imgCnt,
    //                            (XrSwapchainImageBaseHeader *)img_gles);

    uint32_t imageCount;
    CheckXrResult(
        xrEnumerateSwapchainImages(this->swapchain, 0, &imageCount, nullptr));
    this->swapchainImages.resize(imageCount, defaultImage);

    std::vector<XrSwapchainImageBaseHeader *> pointers;
    for (auto &image : this->swapchainImages) {
      pointers.push_back((XrSwapchainImageBaseHeader *)&image);
    }
    CheckXrResult(xrEnumerateSwapchainImages(this->swapchain, imageCount,
                                             &imageCount, pointers[0]));
  }

  ~Swapchain() { xrDestroySwapchain(this->swapchain); }

  std::tuple<uint32_t, T, XrCompositionLayerProjectionView>
  AcquireSwapchain(const XrView &view) {
    XrSwapchainImageAcquireInfo acquireInfo{
        .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
    };
    uint32_t swapchainImageIndex;
    CheckXrResult(xrAcquireSwapchainImage(this->swapchain, &acquireInfo,
                                          &swapchainImageIndex));

    XrSwapchainImageWaitInfo waitInfo{
        .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
        .timeout = XR_INFINITE_DURATION,
    };
    CheckXrResult(xrWaitSwapchainImage(this->swapchain, &waitInfo));

    return {
        swapchainImageIndex,
        this->swapchainImages[swapchainImageIndex],
        {
            .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
            .pose = view.pose,
            .fov = view.fov,
            .subImage =
                {
                    .swapchain = this->swapchain,
                    .imageRect =
                        {
                            .offset = {0, 0},
                            .extent = {.width = static_cast<int32_t>(
                                           this->swapchainCreateInfo.width),
                                       .height = static_cast<int32_t>(
                                           this->swapchainCreateInfo.height)},
                        },
                },
        }};
  }

  void EndSwapchain() {
    XrSwapchainImageReleaseInfo releaseInfo{
        .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
    };
    CheckXrResult(xrReleaseSwapchainImage(this->swapchain, &releaseInfo));
  }
};

} // namespace xr
} // namespace vuloxr

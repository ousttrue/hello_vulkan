#include "xr_vulkan_session.h"
#include <thread>
#include <vuloxr/vk/swapchain.h>
#include <vuloxr/xr.h>
#include <vuloxr/xr/graphics/vulkan.h>
#include <vuloxr/xr/session.h>

// List of supported color swapchain formats.
constexpr VkFormat SupportedColorSwapchainFormats[] = {
    VK_FORMAT_B8G8R8A8_SRGB,
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_FORMAT_B8G8R8A8_UNORM,
    VK_FORMAT_R8G8B8A8_UNORM,
};

int main(int argc, char *argv[]) {
  // Spawn a thread to wait for a keypress
  static bool quitKeyPressed = false;
  auto exitPollingThread = std::thread{[] {
    vuloxr::Logger::Info("Press [enter key] to shutdown...");
    (void)getchar();
    quitKeyPressed = true;
  }};
  exitPollingThread.detach();

  vuloxr::xr::Instance xr_instance;
  xr_instance.extensions.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
  if (xr_instance.create(nullptr) != XR_SUCCESS) {
    vuloxr::Logger::Info("no xro::Instance. no Oculus link ? shutdown...");
    return 1;
  }

  {
    auto [instance, physicalDevice, device] =
        vuloxr::xr::createVulkan(xr_instance.instance, xr_instance.systemId);

    {
      // XrSession
      XrGraphicsBindingVulkan2KHR graphicsBinding{
          .type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR,
          .next = nullptr,
          .instance = instance,
          .physicalDevice = physicalDevice,
          .device = device,
          .queueFamilyIndex = physicalDevice.graphicsFamilyIndex,
          .queueIndex = 0,
      };

      vuloxr::xr::Session session(xr_instance.instance, xr_instance.systemId,
                                  &graphicsBinding);

      XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{
          .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
          .next = 0,
          .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
          .poseInReferenceSpace = {.orientation = {0, 0, 0, 1.0f},
                                   .position = {0, 0, 0}},
      };
      XrSpace appSpace;
      vuloxr::xr::CheckXrResult(xrCreateReferenceSpace(
          session, &referenceSpaceCreateInfo, &appSpace));

      VkClearColorValue clearColor{
          .float32 = {0, 0, 0, 0},
      };
      xr_vulkan_session(
          [pQuit = &quitKeyPressed](bool isSessionRunning) {
            if (*pQuit) {
              return false;
            }
            return true;
          },
          xr_instance.instance, xr_instance.systemId, session, appSpace,
          *vuloxr::vk::selectColorSwapchainFormat(
              session.formats, SupportedColorSwapchainFormats),
          physicalDevice, physicalDevice.graphicsFamilyIndex, device,
          //
          clearColor);

      // session
    }
    // vulkan
  }

  return 0;
}

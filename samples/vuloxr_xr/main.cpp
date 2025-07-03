#include "xr_main_loop.h"
#include <thread>
#include <vuloxr/xr.h>
#include <vuloxr/xr/graphics/vulkan.h>
#include <vuloxr/xr/session.h>

// #include <vuloxr/vk/swapchain.h>

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
    auto vulkan =
        vuloxr::xr::createVulkan(xr_instance.instance, xr_instance.systemId);

    {
      // XrSession
      XrGraphicsBindingVulkan2KHR graphicsBinding{
          .type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR,
          .next = nullptr,
          .instance = vulkan.instance,
          .physicalDevice = vulkan.physicalDevice,
          .device = vulkan.device,
          .queueFamilyIndex = vulkan.physicalDevice.graphicsFamilyIndex,
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

      xr_main_loop(
          [pQuit = &quitKeyPressed](bool isSessionRunning) {
            if (*pQuit) {
              return false;
            }
            return true;
          },
          xr_instance.instance, xr_instance.systemId, session, appSpace,
          session.formats, vulkan, {0, 0, 0, 0});

      // session
    }
    // vulkan
  }

  return 0;
}

#include "xr_vulkan_session.h"
#include <thread>
#include <vuloxr/xr.h>
#include <vuloxr/xr/session.h>
#include <vuloxr/xr/vulkan.h>

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
      vuloxr::xr::Session session(xr_instance.instance, xr_instance.systemId,
                                  instance, physicalDevice,
                                  physicalDevice.graphicsFamilyIndex, device);

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
          session.selectColorSwapchainFormat(), physicalDevice,
          physicalDevice.graphicsFamilyIndex, device,
          //
          clearColor);

      // session
    }
    // vulkan
  }

  return 0;
}

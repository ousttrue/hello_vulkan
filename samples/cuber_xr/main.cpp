#include "xr_main_loop.h"
#include <thread>
#include <vuloxr/xr.h>

#ifdef XR_USE_GRAPHICS_API_VULKAN
#include <vuloxr/xr/graphics/vulkan.h>
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL
#include <vuloxr/xr/graphics/gl.h>
#endif

#include <vuloxr/xr/session.h>

XrColor4f clearColor{0, 0.1f, 0, 0};

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

#ifdef XR_USE_GRAPHICS_API_VULKAN
  xr_instance.extensions.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
#define createGraphics vuloxr::xr::createVulkan
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL
  xr_instance.extensions.push_back(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
#define createGraphics vuloxr::xr::createGl
#endif

  if (xr_instance.create(nullptr) != XR_SUCCESS) {
    vuloxr::Logger::Info("no xro::Instance. no Oculus link ? shutdown...");
    return 1;
  }

  {
    auto [vulkan, binding] =
        createGraphics(xr_instance.instance, xr_instance.systemId);

    {

      vuloxr::xr::Session session(xr_instance.instance, xr_instance.systemId,
                                  &binding);
      XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{
          .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
          .next = 0,
          .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR,
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
          session.formats, vulkan, clearColor);

      // session
    }
    // vulkan
  }

  return 0;
}

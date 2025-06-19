#include "main_loop.h"
#include <vuloxr/glfw.h>

auto NAME = "vko";

int main(int argc, char **argv) {
#if NDEBUG
  bool useDebug = false;
  vulolxr::Logger::Info("## NDEBUG ##");
#else
  bool useDebug = true;
  vuloxr::Logger::Info("## DEBUG ##");
#endif

  vuloxr::glfw::Glfw glfw;
  auto window = glfw.createWindow(640, 480, NAME);

  auto [instance, physicalDevice, device, swapchain] =
      glfw.createVulkan(useDebug);
  main_loop([&glfw]() { return glfw.newFrame(); }, swapchain, physicalDevice,
            device);

  return 0;
}

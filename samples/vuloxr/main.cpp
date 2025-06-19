#include "main_loop.h"
#include <vuloxr/glfw.h>

auto NAME = "vuloxr";

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
  main_loop([&glfw]() { return glfw.newFrame(); }, instance, swapchain,
            physicalDevice, device, window);

  return 0;
}

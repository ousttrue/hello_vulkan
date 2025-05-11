#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

class Glfw {
  GLFWwindow *_window = nullptr;

public:
  Glfw();
  ~Glfw();
  void createWindow(int width, int height, const char *title);
  void addRequiredExtensions(std::vector<const char *> &extensions);
  bool nextFrame(int *width, int *height);
  void flush();
};

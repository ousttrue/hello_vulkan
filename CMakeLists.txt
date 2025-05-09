# project
cmake_minimum_required(VERSION 3.20.0)
project(hello_vulkan VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  # Git auto-ignore out-of-source build directory
  file(GENERATE OUTPUT .gitignore CONTENT "*")
endif()

include(FetchContent)

# dependencies
set(BUILD_SHARED_LIBS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
FetchContent_Declare(
  glfw
  URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
)
FetchContent_MakeAvailable(glfw)

FetchContent_Populate(
  glm
  URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip
)

# target
subdirs(samples/vulkan_tutorial)

# project
cmake_minimum_required(VERSION 4.0)
project(hello_vulkan VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  # Git auto-ignore out-of-source build directory
  file(GENERATE OUTPUT .gitignore CONTENT "*")
endif()

include(FetchContent)

# target
subdirs(samples)

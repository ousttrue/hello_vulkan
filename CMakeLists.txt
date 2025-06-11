# project
cmake_minimum_required(VERSION 3.30)
project(hello_vulkan VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_POLICY_DEFAULT_CMP0148 OLD)
add_compile_options(# stop embed warning
                    -Wno-c23-extensions)

if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  # Git auto-ignore out-of-source build directory
  file(
    GENERATE
    OUTPUT .gitignore
    CONTENT "*")
endif()

include(FetchContent)

# target
subdirs(samples)

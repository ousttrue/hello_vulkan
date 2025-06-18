# project
cmake_minimum_required(VERSION 3.30)
project(hello_vulkan VERSION 0.1.0)

if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  # Git auto-ignore out-of-source build directory
  file(
    GENERATE
    OUTPUT .gitignore
    CONTENT "*")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_POLICY_DEFAULT_CMP0148 OLD)
add_compile_options(# stop embed warning
                    -Wno-c23-extensions -D_CRT_SECURE_NO_WARNINGS)

# $ENV{VULKAN_SDK}
find_package(
  Vulkan REQUIRED # COMPONENTS glslang glslangValidator SPIRV-Tools
                  # shaderc_combined
)

include(FetchContent)
set(BUILD_SHARED_LIBS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.3.8)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  magic_enum
  URL https://github.com/Neargye/magic_enum/releases/download/v0.9.7/magic_enum-v0.9.7.tar.gz
)
FetchContent_MakeAvailable(magic_enum)

# target
subdirs(samples vuloxr)

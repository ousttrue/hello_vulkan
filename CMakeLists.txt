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

set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS false)
# set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/prefix/cmake)
set(CMAKE_INSTALL_PREFIX
    ${CMAKE_CURRENT_LIST_DIR}/prefix
    CACHE PATH "..." FORCE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_POLICY_DEFAULT_CMP0148 OLD)
add_compile_options(# stop embed warning
                    -Wno-c23-extensions -D_CRT_SECURE_NO_WARNINGS)

#
# dependencies
#

include(FetchContent)

FetchContent_Populate(
  stb URL https://github.com/nothings/stb/archive/refs/heads/master.zip)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
target_compile_definitions(stb INTERFACE STB_IMAGE_IMPLEMENTATION)

FetchContent_Declare(
  magic_enum
  URL https://github.com/Neargye/magic_enum/releases/download/v0.9.7/magic_enum-v0.9.7.tar.gz
)
FetchContent_MakeAvailable(magic_enum)

FetchContent_Declare(
  DirectXMath
  URL https://github.com/microsoft/DirectXMath/archive/refs/tags/apr2025.zip)
FetchContent_MakeAvailable(DirectXMath)

# https://raw.githubusercontent.com/dotnet/runtime/main/src/coreclr/pal/inc/rt/sal.h
add_library(sal INTERFACE)
target_include_directories(sal INTERFACE ${CMAKE_CURRENT_LIST_DIR}/deps/sal/inc)

set(BUILD_SHARED_LIBS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.3.8)
FetchContent_MakeAvailable(glfw)

add_library(glext INTERFACE)
target_include_directories(glext INTERFACE ${CMAKE_CURRENT_LIST_DIR}/deps/glext)

find_package(OpenXR REQUIRED)
find_package(GLEW REQUIRED)
find_package(sdl2 REQUIRED)
find_package(Vulkan REQUIRED) # $ENV{VULKAN_SDK}
find_package(glslang REQUIRED)
find_package(shaderc_util REQUIRED)
find_package(shaderc REQUIRED)

subdirs(vuloxr cuber samples)

cmake_minimum_required(VERSION 3.30)
project(hello_vulkan VERSION 0.1.0)

include(prefix.cmake)
message(STATUS "PREFIX => ${CMAKE_INSTALL_PREFIX}")
message(STATUS "BUILD => ${CMAKE_CURRENT_BINARY_DIR}")

if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  # Git auto-ignore out-of-source build directory
  file(
    GENERATE
    OUTPUT .gitignore
    CONTENT "*")
endif()

set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS false)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_POLICY_DEFAULT_CMP0148 OLD)
add_compile_options(# stop embed warning
                    -Wno-c23-extensions -D_CRT_SECURE_NO_WARNINGS)

#
# dependencies
#
# https://raw.githubusercontent.com/dotnet/runtime/main/src/coreclr/pal/inc/rt/sal.h
add_library(sal INTERFACE)
target_include_directories(sal INTERFACE ${CMAKE_CURRENT_LIST_DIR}/deps/sal/inc)

add_library(glext INTERFACE)
target_include_directories(glext INTERFACE ${CMAKE_CURRENT_LIST_DIR}/deps/glext)

find_package(OpenXR REQUIRED)
find_package(GLEW REQUIRED)
find_package(sdl2 REQUIRED)
find_package(Vulkan REQUIRED) # $ENV{VULKAN_SDK}
find_package(glslang REQUIRED)
find_package(shaderc_util REQUIRED)
find_package(shaderc REQUIRED)
find_package(stb REQUIRED)
find_package(magic_enum REQUIRED)
find_package(glfw3 REQUIRED)
find_package(directxmath REQUIRED)

subdirs(vuloxr cuber samples)


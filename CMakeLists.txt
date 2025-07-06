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

#
# dependencies
#

# $ENV{VULKAN_SDK}
find_package(
  Vulkan REQUIRED # COMPONENTS glslang glslangValidator SPIRV-Tools
                  # shaderc_combined
)

include(FetchContent)

if(ANDROID)

  set(TARGET_NAME android_native_app_glue)
  add_library(
    ${TARGET_NAME} STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
  target_link_options(${TARGET_NAME} PUBLIC -u ANativeActivity_onCreate)
  target_include_directories(
    ${TARGET_NAME} PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)

else()
  FetchContent_Declare(zlib URL https://zlib.net/zlib-1.3.1.tar.gz)
  FetchContent_MakeAvailable(zlib)

  set(BUILD_SHARED_LIBS OFF)
  set(GLFW_BUILD_EXAMPLES OFF)
  set(GLFW_BUILD_TESTS OFF)
  set(GLFW_BUILD_DOCS OFF)
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8)
  FetchContent_MakeAvailable(glfw)

  FetchContent_Populate(
    imgui URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b.zip)
  set(TARGET_NAME imgui)
  add_library(
    ${TARGET_NAME} STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
  target_include_directories(${TARGET_NAME} PUBLIC ${imgui_SOURCE_DIR}
                                                   ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(${TARGET_NAME} PUBLIC Vulkan::Vulkan glfw)

  FetchContent_Populate(
    glew
    URL https://sourceforge.net/projects/glew/files/glew/2.1.0/glew-2.1.0.tgz/download
  )
  add_library(glew_s STATIC ${glew_SOURCE_DIR}/src/glew.c
                            ${glew_SOURCE_DIR}/src/glewinfo.c)
  target_include_directories(glew_s PUBLIC ${glew_SOURCE_DIR}/include)
  target_compile_definitions(
    glew_s
    PRIVATE GLEW_BUILD
    PUBLIC GLEW_STATIC)
  target_link_libraries(glew_s PUBLIC OpenGL32)

  # FetchContent_Populate( glext GIT_REPOSITORY
  # https://github.com/KhronosGroup/OpenGL-Registry GIT_TAG main)
  # add_library(glext INTERFACE) target_include_directories(glext INTERFACE
  # ${glext_SOURCE_DIR}/api ${glext_SOURCE_DIR}/extensions)

endif()

FetchContent_Declare(
  magic_enum
  URL https://github.com/Neargye/magic_enum/releases/download/v0.9.7/magic_enum-v0.9.7.tar.gz
)
FetchContent_MakeAvailable(magic_enum)

set(JSONCPP_WITH_TESTS OFF)
FetchContent_Declare(
  jsoncpp
  URL https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.9.6.zip
)
FetchContent_MakeAvailable(jsoncpp)

FetchContent_Populate(
  glm URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip
  # URL
  # https://github.com/g-truc/glm/releases/download/1.0.1/glm-1.0.1-light.zip
)
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE ${glm_SOURCE_DIR})

FetchContent_Populate(
  stb URL https://github.com/nothings/stb/archive/refs/heads/master.zip)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
target_compile_definitions(stb INTERFACE STB_IMAGE_IMPLEMENTATION)

FetchContent_Declare(
  openxr
  GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
  GIT_TAG release-1.1.47)
FetchContent_MakeAvailable(openxr)

FetchContent_Declare(
  SPIRV-Headers
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
  GIT_TAG vulkan-sdk-1.4.313.0)
FetchContent_MakeAvailable(SPIRV-Headers)

# set(SPIRV_TOOLS_BUILD_STATIC ON)
FetchContent_Declare(
  SPIRV-Tools
  URL https://github.com/KhronosGroup/SPIRV-Tools/archive/refs/tags/v2025.1.zip)
FetchContent_MakeAvailable(SPIRV-Tools)

FetchContent_Declare(
  glslang
  URL https://github.com/KhronosGroup/glslang/archive/refs/tags/15.3.0.zip)
FetchContent_MakeAvailable(glslang)

set(SHADERC_SKIP_TESTS ON)
set(SHADERC_SKIP_INSTALL ON)
FetchContent_Declare(
  shaderc
  GIT_REPOSITORY https://github.com/google/shaderc.git
  GIT_TAG v2025.2)
FetchContent_MakeAvailable(shaderc)

FetchContent_Declare(
  DirectXMath
  URL https://github.com/microsoft/DirectXMath/archive/refs/tags/apr2025.zip)
FetchContent_MakeAvailable(DirectXMath)

# https://raw.githubusercontent.com/dotnet/runtime/main/src/coreclr/pal/inc/rt/sal.h
add_library(sal INTERFACE)
target_include_directories(sal INTERFACE ${CMAKE_CURRENT_LIST_DIR}/deps/sal/inc)

add_library(glext INTERFACE)
target_include_directories(glext INTERFACE ${CMAKE_CURRENT_LIST_DIR}/deps/glext)

#
# subdirs
#
subdirs(samples vuloxr)

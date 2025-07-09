cmake_minimum_required(VERSION 3.20.0)
project(deps)

include(../prefix.cmake)

set(CMAKE_CXX_STANDARD 20)
cmake_policy(SET CMP0177 NEW)
cmake_policy(SET CMP0135 NEW)

# for compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# for build/.gitignore
if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  file(
    GENERATE
    OUTPUT .gitignore
    CONTENT "*")
endif()

# for prefix/.gitignore
file(
  GENERATE
  OUTPUT ${CMAKE_INSTALL_PREFIX}/.gitignore
  CONTENT "*")

add_compile_options(# stop embed warning
                    -Wno-c23-extensions -D_CRT_SECURE_NO_WARNINGS)

#
# dependencies
#
include(FetchContent)

#
# magic enum
#
set(MAGIC_ENUM_OPT_INSTALL ON)
FetchContent_Declare(
  magic_enum
  URL https://github.com/Neargye/magic_enum/releases/download/v0.9.7/magic_enum-v0.9.7.tar.gz
)
FetchContent_MakeAvailable(magic_enum)

#
# openxr
#
FetchContent_Declare(
  openxr
  GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
  GIT_TAG release-1.1.47)
FetchContent_MakeAvailable(openxr)

#
# glew
#
FetchContent_Declare(
  glew
  # URL
  URL https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip
      SOURCE_SUBDIR
      build/cmake
  PATCH_COMMAND
    ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/glew.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/_deps/glew-src/build/cmake/CMakeLists.txt
  UPDATE_DISCONNECTED 1)
FetchContent_MakeAvailable(glew)

#
# glfw
#
set(BUILD_SHARED_LIBS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.3.8)
FetchContent_MakeAvailable(glfw)

#
# SDL2
#
FetchContent_Declare(
  sdl2
  URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.2/SDL2-2.32.2.zip
)
FetchContent_MakeAvailable(sdl2)
install(
  TARGETS SDL2-static
  EXPORT sdl2-config
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)
install(
  EXPORT sdl2-config
  FILE sdl2-config.cmake
  DESTINATION cmake)
install(DIRECTORY ${sdl2_SOURCE_DIR}/include/ DESTINATION include/SDL2)

#
# shaderc
#
FetchContent_Declare(
  SPIRV-Headers
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
  GIT_TAG vulkan-sdk-1.4.313.0)
FetchContent_MakeAvailable(SPIRV-Headers)

# set(SPIRV_TOOLS_BUILD_STATIC ON)
set(SPIRV_SKIP_TESTS ON)
FetchContent_Declare(
  SPIRV-Tools
  URL https://github.com/KhronosGroup/SPIRV-Tools/archive/refs/tags/v2025.1.zip)
FetchContent_MakeAvailable(SPIRV-Tools)

set(GLSLANG_TESTS OFF)
set(GLSLANG_ENABLE_INSTALL ON)
FetchContent_Declare(
  glslang
  URL https://github.com/KhronosGroup/glslang/archive/refs/tags/15.3.0.zip)
FetchContent_MakeAvailable(glslang)

set(SHADERC_SKIP_TESTS ON)
set(SHADERC_SKIP_INSTALL OFF)
FetchContent_Declare(
  shaderc
  GIT_REPOSITORY https://github.com/google/shaderc.git
  GIT_TAG v2025.2)
FetchContent_MakeAvailable(shaderc)

set_target_properties(shaderc PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "")
target_include_directories(
  shaderc
  PUBLIC #
         $<BUILD_INTERFACE:${shaderc_SOURCE_DIR}/libshaderc/include>
         $<BUILD_INTERFACE:${shaderc_SOURCE_DIR}/libshaderc_util/include>
         $<INSTALL_INTERFACE:include>)

set_target_properties(shaderc_util PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "")
target_include_directories(
  shaderc_util
  PUBLIC #
         $<BUILD_INTERFACE:${shaderc_SOURCE_DIR}/libshaderc/include>
         $<BUILD_INTERFACE:${shaderc_SOURCE_DIR}/libshaderc_util/include>
         $<INSTALL_INTERFACE:include>)

install(TARGETS shaderc EXPORT shaderc-config)
install(
  EXPORT shaderc-config
  FILE shaderc-config.cmake
  DESTINATION cmake)

install(TARGETS shaderc_util EXPORT shaderc_util-config)
install(
  EXPORT shaderc_util-config
  FILE shaderc_util-config.cmake
  DESTINATION cmake)

message(STATUS "PREFIX => ${CMAKE_INSTALL_PREFIX}")
message(STATUS "BUILD => ${CMAKE_CURRENT_BINARY_DIR}")

#
# stb
#
FetchContent_Populate(
  stb URL https://github.com/nothings/stb/archive/refs/heads/master.zip)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)
add_library(stb INTERFACE)
set_target_properties(stb PROPERTIES PUBLIC_HEADER
                                     ${stb_SOURCE_DIR}/stb_image.h)
target_compile_definitions(stb INTERFACE STB_IMAGE_IMPLEMENTATION)
install(
  TARGETS stb
  EXPORT stb-config
  INCLUDES
  DESTINATION include
  PUBLIC_HEADER DESTINATION include/stb)
install(
  EXPORT stb-config
  FILE stb-config.cmake
  DESTINATION cmake)

#
# DirectXMath
#
FetchContent_Declare(
  DirectXMath
  URL https://github.com/microsoft/DirectXMath/archive/refs/tags/apr2025.zip)
FetchContent_MakeAvailable(DirectXMath)

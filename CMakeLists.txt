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
if(ANDROID)

else()
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

# target
subdirs(samples vuloxr)

# dependencies
set(BUILD_SHARED_LIBS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)

FetchContent_Populate(
  glm URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip
  # URL
  # https://github.com/g-truc/glm/releases/download/1.0.1/glm-1.0.1-light.zip
)

FetchContent_Populate(
  stb URL https://github.com/nothings/stb/archive/refs/heads/master.zip)

FetchContent_Populate(
  magic_enum
  URL https://github.com/Neargye/magic_enum/releases/download/v0.9.7/magic_enum-v0.9.7.tar.gz
)

FetchContent_Declare(
  openxr
  GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
  GIT_TAG release-1.1.47)
FetchContent_MakeAvailable(openxr)

function(add_glsl TARGET SHADER)
  find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Include/Bin)
  set(current-input-path ${CMAKE_CURRENT_LIST_DIR}/${SHADER})
  set(current-output-path ${CMAKE_CURRENT_BINARY_DIR}/${SHADER}.spv)
  add_custom_command(
    OUTPUT ${current-output-path}
    COMMAND ${GLSLC} -o ${current-output-path} ${current-input-path}
    DEPENDS ${current-input-path}
    IMPLICIT_DEPENDS CXX ${current-shader-path}
    VERBATIM)
  set_source_files_properties(${current-output-path} PROPERTIES GENERATED TRUE)
  target_sources(${TARGET} PRIVATE ${current-output-path})
endfunction(add_glsl)

if(ANDROID_NDK)
  set(TARGET_NAME android_native_app_glue)
  add_library(
    ${TARGET_NAME} STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
  target_link_options(${TARGET_NAME} PUBLIC -u ANativeActivity_onCreate)
  target_include_directories(
    ${TARGET_NAME} PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)

  FetchContent_Declare(
    SPIRV-Headers
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
    GIT_TAG vulkan-sdk-1.4.313.0)
  FetchContent_MakeAvailable(SPIRV-Headers)

  FetchContent_Declare(
    SPIRV-Tools
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
    GIT_TAG vulkan-sdk-1.4.313.0)
  FetchContent_MakeAvailable(SPIRV-Tools)

  FetchContent_Declare(
    glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG vulkan-sdk-1.4.313.0)
  FetchContent_MakeAvailable(glslang)

  set(SHADERC_SKIP_INSTALL OFF)
  set(SHADERC_SKIP_TESTS ON)
  set(SHADERC_SKIP_INSTALL ON)
  FetchContent_Declare(
    shaderc
    GIT_REPOSITORY https://github.com/google/shaderc.git
    GIT_TAG v2025.2)
  FetchContent_MakeAvailable(shaderc)

  # https://developer.android.com/ndk/guides/graphics/shader-compilers
  subdirs(hellotriangle/cpp)
else()
  set(TARGET_NAME vulkan)
  add_library(${TARGET_NAME} STATIC IMPORTED GLOBAL)
  set_target_properties(
    ${TARGET_NAME}
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES $ENV{VULKAN_SDK}/Include
               IMPORTED_LOCATION $ENV{VULKAN_SDK}/Lib/vulkan-1.lib)
  subdirs(vulkan_tutorial)

  set(TARGET_NAME shaderc)
  add_library(${TARGET_NAME} STATIC IMPORTED GLOBAL)
  set_target_properties(
    ${TARGET_NAME}
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES $ENV{VULKAN_SDK}/Include
               IMPORTED_LOCATION $ENV{VULKAN_SDK}/Lib/shadercd.lib)
  set_property(
    TARGET ${TARGET_NAME}
    PROPERTY INTERFACE_LINK_LIBRARIES
             $ENV{VULKAN_SDK}/Lib/shaderc_utild.lib
             $ENV{VULKAN_SDK}/Lib/SPIRV-Toolsd.lib
             $ENV{VULKAN_SDK}/Lib/SPIRV-Tools-optd.lib
             $ENV{VULKAN_SDK}/Lib/glslangd.lib)

endif()

subdirs(vulfwk hello_xr/cpp)

# dependencies
subdirs(xro)

# FetchContent_Declare( jsoncpp URL
# https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.9.6.zip )
# FetchContent_MakeAvailable(jsoncpp)

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

function(add_glsl TARGET SHADER)
  find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin)
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

subdirs(vko)
if(ANDROID_NDK)
  find_package(Vulkan REQUIRED)

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

  # https://developer.android.com/ndk/guides/graphics/shader-compilers
  subdirs(ovr_openxr_mobile_sdk)
else()
  FetchContent_Declare(zlib URL https://zlib.net/zlib-1.3.1.tar.gz)
  FetchContent_MakeAvailable(zlib)
endif()

FetchContent_Declare(
  SPIRV-Headers
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
  GIT_TAG vulkan-sdk-1.4.313.0)
FetchContent_MakeAvailable(SPIRV-Headers)

FetchContent_Declare(
  SPIRV-Tools
  URL https://github.com/KhronosGroup/SPIRV-Tools/archive/refs/tags/v2025.1.zip)
FetchContent_MakeAvailable(SPIRV-Tools)

FetchContent_Declare(
  glslang
  URL https://github.com/KhronosGroup/glslang/archive/refs/tags/15.3.0.zip)
FetchContent_MakeAvailable(glslang)

set(SHADERC_SKIP_INSTALL OFF)
set(SHADERC_SKIP_TESTS ON)
set(SHADERC_SKIP_INSTALL ON)
FetchContent_Declare(
  shaderc
  GIT_REPOSITORY https://github.com/google/shaderc.git
  GIT_TAG v2025.2)
FetchContent_MakeAvailable(shaderc)

subdirs(#
        hello_xr/cpp vuloxr)

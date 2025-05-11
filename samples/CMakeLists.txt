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

function(add_shader TARGET SHADER)
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
endfunction(add_shader)

if(ANDROID_NDK)
  set(TARGET_NAME android_native_app_glue)
  add_library(
    ${TARGET_NAME} STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
  target_link_options(${TARGET_NAME} PUBLIC -u ANativeActivity_onCreate)
  target_include_directories(
    ${TARGET_NAME} PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)

  subdirs(hellotriangle)
else()
  set(TARGET_NAME vulkan_sdk)
  add_library(${TARGET_NAME} STATIC IMPORTED GLOBAL)
  set_target_properties(
    ${TARGET_NAME}
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES $ENV{VULKAN_SDK}/Include
               IMPORTED_LOCATION $ENV{VULKAN_SDK}/Lib/vulkan-1.lib)

  subdirs(vulkan_tutorial)
endif()

subdirs(vulfwk)

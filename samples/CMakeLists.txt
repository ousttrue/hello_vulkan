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

if(${PLATFORM} MATCHES "android")
  set(TARGET_NAME android_native_app_glue)
  add_library(
    ${TARGET_NAME} STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
  target_link_options(${TARGET_NAME} PRIVATE -u ANativeActivity_onCreate)
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

  subdirs(vulkan_tutorial deps_win32)
endif()

subdirs(vulfwk)

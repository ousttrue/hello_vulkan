set(TARGET_NAME openxr_gl)
add_library(
  ${TARGET_NAME} STATIC
  assertgl.c
  util_matrix.c
  util_render2d.c
  util_render_target.c
  util_shader.c
  util_texture.c
  winsys/winsys_null.c)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})

if(ANDROID)
  target_compile_definitions(
    ${TARGET_NAME} PRIVATE XR_OS_ANDROID XR_USE_PLATFORM_ANDROID
                          XR_USE_GRAPHICS_API_OPENGL_ES)

  target_link_libraries(
    ${TARGET_NAME}
    PUBLIC OpenXR::openxr_loader
           stb
           #
           android_native_app_glue
           android
           EGL
           GLESv3
           log)
else()

  target_compile_definitions(
    ${TARGET_NAME} PRIVATE XR_OS_WINDOWS XR_USE_PLATFORM_WIN32
                          XR_USE_GRAPHICS_API_OPENGL)
  target_link_libraries(
    ${TARGET_NAME}
    PUBLIC OpenXR::openxr_loader stb
           #
           glew_s)
endif()

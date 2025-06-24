set(TARGET_NAME android_openxr_gles)
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
target_compile_definitions(
  ${TARGET_NAME} PUBLIC XR_OS_ANDROID XR_USE_PLATFORM_ANDROID
                        XR_USE_GRAPHICS_API_OPENGL_ES)

target_link_libraries(
  ${TARGET_NAME}
  PUBLIC android_native_app_glue
         OpenXR::openxr_loader
         android
         EGL
         GLESv3
         log
         stb)

set(TARGET_NAME gl2triOXR)
add_library(${TARGET_NAME} SHARED android_main.cpp app_engine.cpp
                                  render_scene.cpp)
target_compile_definitions(${TARGET_NAME} PRIVATE -DXR_USE_PLATFORM_ANDROID
                                                  -DXR_USE_GRAPHICS_API_VULKAN)
target_link_libraries(
  ${TARGET_NAME}
  PRIVATE android log android_native_app_glue vulkan OpenXR::openxr_loader
          # android_openxr_gles
          vuloxr)

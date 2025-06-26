set(TARGET_NAME gl2triOXR)
add_library(${TARGET_NAME} SHARED android_main.cpp xr_session.cpp
                                  ViewRenderer.cpp)
target_link_libraries(
  ${TARGET_NAME} PRIVATE android log android_native_app_glue
                         OpenXR::openxr_loader android_openxr_gles vuloxr)

set(TARGET_NAME cuber_xr)
add_executable(
  ${TARGET_NAME}
  main.cpp
  #
  xr_main_loop.cpp
  # render_scene.cpp
)
target_link_libraries(${TARGET_NAME} PRIVATE vuloxr OpenXR::openxr_loader
                                             openxr_gl cuber)
target_compile_definitions(${TARGET_NAME} PRIVATE XR_USE_PLATFORM_WIN32
                                                  XR_USE_GRAPHICS_API_OPENGL)

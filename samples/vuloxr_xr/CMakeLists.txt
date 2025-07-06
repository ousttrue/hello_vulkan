subdirs(openxr_gl android_openxr_gles_imgui)

include(func_vk.cmake)
include(func_gl.cmake)

add_gl_sample(hello_xr)
add_vk_sample(hello_xr)

# add_gl_sample(gl2triOXR) add_vk_sample(gl2triOXR)

set(TARGET_NAME gl2triOXR)
add_executable(
  ${TARGET_NAME}
  main.cpp
  #
  gl2triOXR/xr_main_loop.cpp
  gl2triOXR/app_engine.cpp
  gl2triOXR/render_scene.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE vuloxr OpenXR::openxr_loader
                                             openxr_gl)
target_compile_definitions(${TARGET_NAME} PRIVATE XR_USE_PLATFORM_WIN32
                                                  XR_USE_GRAPHICS_API_OPENGL)

set(TARGET_NAME gl2tri3dOXR)
add_executable(
  ${TARGET_NAME}
  main.cpp
  #
  gl2tri3dOXR/xr_main_loop.cpp
  gl2tri3dOXR/app_engine.cpp
  gl2tri3dOXR/render_scene.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE vuloxr OpenXR::openxr_loader
                                             openxr_gl)
target_compile_definitions(${TARGET_NAME} PRIVATE XR_USE_PLATFORM_WIN32
                                                  XR_USE_GRAPHICS_API_OPENGL)

set(TARGET_NAME gl2teapotOXR)
add_executable(
  ${TARGET_NAME}
  main.cpp
  #
  gl2teapotOXR/xr_main_loop.cpp
  gl2teapotOXR/app_engine.cpp
  gl2teapotOXR/render_scene.cpp
  gl2teapotOXR/teapot.cpp
  gl2teapotOXR/teapot_data.cpp
)
target_link_libraries(${TARGET_NAME} PRIVATE vuloxr OpenXR::openxr_loader
                                             openxr_gl)
target_compile_definitions(${TARGET_NAME} PRIVATE XR_USE_PLATFORM_WIN32
                                                  XR_USE_GRAPHICS_API_OPENGL)

set(TARGET_NAME gl2imguiOXR)
add_executable(
  ${TARGET_NAME}
  main.cpp
  gl2imguiOXR/xr_main_loop.cpp
  gl2imguiOXR/app_engine.cpp
  gl2imguiOXR/render_scene.cpp
  gl2imguiOXR/render_texplate.cpp
  gl2imguiOXR/render_imgui.cpp
  # gl2imguiOXR/teapot.cpp
  gl2teapotOXR/teapot.cpp
  gl2teapotOXR/teapot_data.cpp
  #
  # gl2imguiOXR/util_oxr.cpp
  # gl2imguiOXR/util_render_target.cpp
)
target_link_libraries(${TARGET_NAME} PRIVATE vuloxr OpenXR::openxr_loader
                                             openxr_gl oxr_imgui)
target_compile_definitions(${TARGET_NAME} PRIVATE XR_USE_PLATFORM_WIN32
                                                  XR_USE_GRAPHICS_API_OPENGL)

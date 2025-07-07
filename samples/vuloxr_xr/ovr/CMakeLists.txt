set(TARGET_NAME ovr)
add_library(
  ${TARGET_NAME} STATIC
  Misc/Log.c
  Input/AxisRenderer.cpp
  Input/HandRenderer.cpp
  Render/GlBuffer.cpp
  Render/Egl.c
  Render/GlWrapperWin32.c
  Render/Framebuffer.cpp
  Render/GlProgram.cpp
  Render/GlGeometry.cpp
  Render/SurfaceRender.cpp
  #
)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(${TARGET_NAME} PUBLIC WIN32)
target_link_libraries(${TARGET_NAME} PUBLIC OpenXR::openxr_loader glext)

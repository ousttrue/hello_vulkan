set(TARGET_NAME cuber)
add_library(
  ${TARGET_NAME} STATIC
  mesh.cpp
  gl3/GlCubeRenderer.cpp
  gl3/GlLineRenderer.cpp
  grapho/vars.cpp
  grapho/camera/camera.cpp
  grapho/camera/ray.cpp
  grapho/gl3/vao.cpp
  grapho/gl3/texture.cpp
  grapho/gl3/shader.cpp
  grapho/gl3/cuberenderer.cpp
  grapho/gl3/fbo.cpp
  grapho/gl3/error_check.cpp)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${TARGET_NAME} PUBLIC GLEW::glew_s)

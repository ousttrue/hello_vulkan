set(TARGET_NAME cuber_sample)
add_executable(
  ${TARGET_NAME}
  main.cpp
  GLfwPlatform.cpp
  GuiApp.cpp
  Bvh.cpp
  BvhSolver.cpp
  BvhNode.cpp
  Animation.cpp
  UdpSender.cpp
  BvhPanel.cpp
  Payload.cpp
  BvhFrame.cpp
)
target_link_libraries(${TARGET_NAME} PRIVATE cuber GLEW::glew_s imgui asio)
target_compile_definitions(${TARGET_NAME} PRIVATE _WIN32_WINNT=0x0601)

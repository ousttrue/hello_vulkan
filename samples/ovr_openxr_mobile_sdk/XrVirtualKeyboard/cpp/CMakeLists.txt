add_executable(
  xrvirtualkeyboard main.cpp VirtualKeyboardModelRenderer.cpp
                    XrRenderModelHelper.cpp XrVirtualKeyboardHelper.cpp)
target_link_libraries(xrvirtualkeyboard samplexrframework)

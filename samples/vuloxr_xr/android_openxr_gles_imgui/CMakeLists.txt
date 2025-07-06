set(TARGET_NAME oxr_imgui)
add_library(
  ${TARGET_NAME} STATIC imgui.cpp imgui_demo.cpp imgui_draw.cpp
                        imgui_widgets.cpp examples/imgui_impl_opengl3.cpp)
target_include_directories(
  ${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}
                        ${CMAKE_CURRENT_LIST_DIR}/examples)
target_compile_definitions(${TARGET_NAME} PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLEW)
target_link_libraries(${TARGET_NAME} PRIVATE glew_s)

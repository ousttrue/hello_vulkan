set(TARGET_NAME openxr-simple-example)
add_executable(${TARGET_NAME} main.cpp glimpl.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE SDL2-static GLEW::glew_s
                                             OpenXR::openxr_loader)

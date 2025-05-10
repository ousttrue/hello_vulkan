set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME vulfwk)
add_executable(${TARGET_NAME} main.cpp platform_win32.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE vulkan_sdk glfw)
target_compile_options(${TARGET_NAME} PRIVATE /EHsc)

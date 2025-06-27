set(TARGET_NAME 05_cube)
add_executable(${TARGET_NAME} main_loop.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE vuloxr shaderc glm Vulkan::Vulkan)

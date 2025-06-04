set(CMAKE_CXX_STANDARD 20)

add_executable(01_instance 01_instance.cpp)
target_link_libraries(01_instance PRIVATE vulkan glfw)

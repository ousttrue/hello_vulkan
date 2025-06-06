set(CMAKE_CXX_STANDARD 20)

add_library(vko INTERFACE)
target_include_directories(vko INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(vko INTERFACE vulkan)

if(ANDROID)
else()

add_executable(01_instance 01_instance.cpp)
target_link_libraries(01_instance PRIVATE vko glfw)

endif()

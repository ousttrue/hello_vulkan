set(TARGET_NAME vulkan_tutorial)

FetchContent_Declare(
  glfw
  URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
)
FetchContent_MakeAvailable(glfw)

SET(CMAKE_CXX_STANDARD 20)
add_executable(${TARGET_NAME}
  main.cpp
)
target_include_directories(${TARGET_NAME} PRIVATE
  $ENV{VULKAN_SDK}/Include
  ${glm_SOURCE_DIR}
)
target_link_directories(${TARGET_NAME} PRIVATE
  $ENV{VULKAN_SDK}/Lib
)
target_link_libraries(${TARGET_NAME} PRIVATE 
  glfw
  vulkan-1.lib
)
add_shader(${TARGET_NAME} shader.vert)
add_shader(${TARGET_NAME} shader.frag)

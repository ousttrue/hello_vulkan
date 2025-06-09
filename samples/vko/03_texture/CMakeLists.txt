set(TARGET_NAME 03_texture)
add_executable(
  ${TARGET_NAME}
  #
  main.cpp
  glfw_app.cpp
  memory_allocator.cpp
  pipeline_object.cpp
  swapchain_command.cpp
  per_image_object.cpp
  ../glsl_to_spv.cpp)
target_compile_options(${TARGET_NAME} PRIVATE /EHsc)
target_link_libraries(${TARGET_NAME} PRIVATE glfw Vulkan::Vulkan stb vko
                                             Vulkan::shaderc_combined)

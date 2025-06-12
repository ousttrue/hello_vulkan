add_library(
  vkr STATIC
  VulkanRenderer.cpp
  MemoryAllocator.cpp
  CmdBuffer.cpp
  DepthBuffer.cpp
  RenderTarget.cpp
  Pipeline.cpp
  RenderPass.cpp
  VertexBuffer.cpp
  vulkan_debug_object_namer.cpp)

target_include_directories(vkr PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

if(ANDROID)
  target_link_libraries(
    vkr
    PRIVATE shaderc common
    PUBLIC Vulkan::Vulkan vko)
else()
  target_link_libraries(
    vkr
    PRIVATE Vulkan::shaderc_combined common
    PUBLIC Vulkan::Vulkan vko)
endif()

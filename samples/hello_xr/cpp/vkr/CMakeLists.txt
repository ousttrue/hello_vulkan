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
)

target_include_directories(vkr PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(
  vkr
  PRIVATE common vko
  PUBLIC Vulkan::Vulkan)

if(ANDROID)
  target_link_libraries(vkr PRIVATE shaderc)

else()
  target_link_libraries(vkr PRIVATE Vulkan::shaderc_combined)

endif()

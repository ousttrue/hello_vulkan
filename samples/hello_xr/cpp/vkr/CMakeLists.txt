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
  #
  vulkan_debug_object_namer.cpp)

target_link_libraries(
  vkr
  PRIVATE shaderc common
  PUBLIC vulkan)

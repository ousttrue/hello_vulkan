add_library(
  openxr_program STATIC
  openxr_program.cpp
  openxr_session.cpp
  GetXrReferenceSpaceCreateInfo.cpp
  VulkanDebugMessageThunk.cpp
  Swapchain.cpp
  options.cpp
  vulkan_layers.cpp)

target_link_libraries(
  openxr_program
  PRIVATE common
  PUBLIC OpenXR::openxr_loader vulkan)

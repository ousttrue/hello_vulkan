set(CMAKE_CXX_STANDARD 20)

add_library(vko INTERFACE)
target_include_directories(vko INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

if(ANDROID)
  function(add_vko_sample TARGET_NAME)
    add_library(
      ${TARGET_NAME} SHARED
      #
      android_main.cpp ${ARGV})
    target_compile_definitions(${TARGET_NAME}
                               PRIVATE ANDROID=1 VK_USE_PLATFORM_ANDROID_KHR=1)
    target_link_libraries(${TARGET_NAME} PRIVATE android_native_app_glue
                                                 android log vko vulkan shaderc)
  endfunction()

else()
  function(add_vko_sample TARGET_NAME)
    add_executable(
      ${TARGET_NAME}
      #
      main.cpp ${ARGV})
    target_link_libraries(${TARGET_NAME} PRIVATE vko Vulkan::Vulkan glfw
                                                 Vulkan::shaderc_combined)
  endfunction()

endif()

add_vko_sample(01_clear 01_clear/main_loop.cpp)
add_vko_sample(02_triangle glsl_to_spv.cpp 02_triangle/main_loop.cpp)
add_vko_sample(
  03_texture
  glsl_to_spv.cpp
  03_texture/main_loop.cpp
  03_texture/memory_allocator.cpp
  03_texture/pipeline_object.cpp
  )

if(ANDROID)
  function(add_vuloxr_sample TARGET_NAME)
    add_library(
      ${TARGET_NAME} SHARED
      #
      android_main.cpp ${ARGV})
    target_compile_definitions(${TARGET_NAME}
                               PRIVATE ANDROID=1 VK_USE_PLATFORM_ANDROID_KHR=1)
    target_link_libraries(
      ${TARGET_NAME} PRIVATE android_native_app_glue android log vuloxr vulkan
                             shaderc)

  endfunction()

else()
  function(add_vuloxr_sample TARGET_NAME)
    add_executable(
      ${TARGET_NAME}
      #
      main.cpp ${ARGV})
    target_link_libraries(${TARGET_NAME} PRIVATE vuloxr Vulkan::Vulkan glfw
                                                 shaderc vuloxr)
  endfunction()

endif()

add_vuloxr_sample(01_clear 01_clear/main_loop.cpp)
add_vuloxr_sample(02_triangle 02_triangle/main_loop.cpp)
add_vuloxr_sample(02_hellotriangle 02_hellotriangle/main_loop.cpp)
add_vuloxr_sample(03_texture 03_texture/main_loop.cpp)

if(ANDROID)

else()
  add_vuloxr_sample(04_imgui 04_imgui/main_loop.cpp)
  target_link_libraries(04_imgui PRIVATE vuloxr imgui jsoncpp_static)

  add_vuloxr_sample(05_cube 05_cube/main_loop.cpp)

endif()

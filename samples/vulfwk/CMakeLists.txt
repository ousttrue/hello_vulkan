set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME vulfwk)

if(ANDROID_NDK)
  add_library(${TARGET_NAME} SHARED main.cpp vulfwk.cpp)
  target_compile_definitions(${TARGET_NAME} PRIVATE ANDROID=1)
  target_link_libraries(${TARGET_NAME} PRIVATE android_native_app_glue android
                                               log vulkan)
else()
  add_executable(${TARGET_NAME} main.cpp platform_win32.cpp vulfwk.cpp)
  target_compile_definitions(${TARGET_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR=1)
  target_link_libraries(${TARGET_NAME} PRIVATE vulkan_sdk glfw)
  target_compile_options(${TARGET_NAME} PRIVATE /EHsc)
  add_shader(${TARGET_NAME} shader.vert)
  add_shader(${TARGET_NAME} shader.frag)
endif()

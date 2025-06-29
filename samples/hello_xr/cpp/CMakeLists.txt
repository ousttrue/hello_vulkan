set(CMAKE_CXX_STANDARD 20)

if(ANDROID)
  add_library(hello_xr MODULE android_main.cpp xr_vulkan_session.cpp)
  target_compile_definitions(hello_xr PUBLIC -DXR_USE_PLATFORM_ANDROID
                                             -DXR_USE_GRAPHICS_API_VULKAN)
  target_link_libraries(
    hello_xr PRIVATE ${ANDROID_LIBRARY} ${ANDROID_LOG_LIBRARY}
                     android_native_app_glue vulkan)
else()
  add_executable(hello_xr main.cpp xr_vulkan_session.cpp)
  if(WIN32)
    target_compile_definitions(
      hello_xr PUBLIC -DXR_USE_PLATFORM_WIN32 -DXR_USE_GRAPHICS_API_VULKAN
                      -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
    target_link_libraries(hello_xr PRIVATE ole32 Vulkan::Vulkan)
    if(MSVC)
      target_compile_definitions(hello_xr PRIVATE _CRT_SECURE_NO_WARNINGS)
      target_compile_options(hello_xr PRIVATE /Zc:wchar_t /Zc:forScope /W4
                                              /EHsc)
    endif()
  endif()

endif()

target_link_libraries(hello_xr PRIVATE vuloxr OpenXR::openxr_loader shaderc)

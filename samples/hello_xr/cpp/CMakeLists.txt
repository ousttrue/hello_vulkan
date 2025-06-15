set(CMAKE_CXX_STANDARD 20)

subdirs(common openxr_program)

if(ANDROID)
  add_library(hello_xr MODULE android_main.cpp xr_loop.cpp glsl_to_spv.cpp)
  target_link_libraries(hello_xr PRIVATE shaderc)
else()
  add_executable(hello_xr main.cpp xr_loop.cpp glsl_to_spv.cpp)
  if(WIN32)
    target_link_libraries(hello_xr PRIVATE ole32 Vulkan::shaderc_combined)
    if(MSVC)
      target_compile_definitions(hello_xr PRIVATE _CRT_SECURE_NO_WARNINGS)
      target_compile_options(hello_xr PRIVATE /Zc:wchar_t /Zc:forScope /W4
                                              /EHsc)
    endif()
  endif()

endif()

target_include_directories(hello_xr PRIVATE .)
target_link_libraries(hello_xr PRIVATE common openxr_program vko)

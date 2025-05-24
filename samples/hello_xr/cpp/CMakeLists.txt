set(CMAKE_CXX_STANDARD 20)

subdirs(vkr)

set(LOCAL_SOURCE
    logger.cpp
    GetXrReferenceSpaceCreateInfo.cpp
    VulkanDebugMessageThunk.cpp
    openxr_program.cpp
    openxr_session.cpp
    Swapchain.cpp
    options.cpp
    vulkan_layers.cpp)

set(VULKAN_SHADERS vulkan_shaders/frag.glsl vulkan_shaders/vert.glsl)
source_group("Shaders" FILES ${VULKAN_SHADERS})
include(compile_glsl.cmake)
compile_glsl(run_hello_xr_glsl_compiles ${VULKAN_SHADERS})

if(ANDROID)
  # https://developer.android.com/ndk/guides/graphics/shader-compilers
  add_library(
    hello_xr MODULE android_main.cpp ${LOCAL_SOURCE} ${VULKAN_SHADERS}
                    $<TARGET_OBJECTS:android_native_app_glue>)
  target_compile_definitions(hello_xr PRIVATE -DXR_USE_PLATFORM_ANDROID
                                              -DXR_USE_GRAPHICS_API_VULKAN)
  target_link_libraries(
    hello_xr PRIVATE ${ANDROID_LIBRARY} ${ANDROID_LOG_LIBRARY}
                     android_native_app_glue)

else()
  add_executable(hello_xr main.cpp ${LOCAL_SOURCE} ${VULKAN_SHADERS})
  target_compile_definitions(
    hello_xr PRIVATE -DXR_USE_PLATFORM_WIN32 -DXR_USE_GRAPHICS_API_VULKAN
                     -DWIN32_LEAN_AND_MEAN -DNOMINMAX)

  if(WIN32)
    target_link_libraries(hello_xr PRIVATE ole32)
    if(MSVC)
      target_compile_definitions(hello_xr PRIVATE _CRT_SECURE_NO_WARNINGS)
      target_compile_options(hello_xr PRIVATE /Zc:wchar_t /Zc:forScope /W4
                                              /EHsc)
      # if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" ) # If actually msvc and
      # not clang-cl target_compile_options(openxr_c_compile_test PRIVATE /WX)
      # endif()

      # Right now can't build d3d features on MinGW because of directxcolors,
      # directxmath, etc.
      target_link_libraries(hello_xr PRIVATE d3d11 d3d12 d3dcompiler dxgi)
    endif()
  endif()

endif()

target_include_directories(hello_xr PRIVATE .)
target_link_libraries(hello_xr PRIVATE OpenXR::openxr_loader vkr)

add_dependencies(hello_xr run_hello_xr_glsl_compiles)

target_include_directories(
  hello_xr
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
          "${PROJECT_SOURCE_DIR}/src/common"
          "${PROJECT_SOURCE_DIR}/src/external/metal-cpp"
          "${PROJECT_SOURCE_DIR}/src/tests/hello_xr/vulkan_shaders"
          # for helper headers
          "${PROJECT_SOURCE_DIR}/external/include"
          # For including compiled shaders
          "${CMAKE_CURRENT_BINARY_DIR}")

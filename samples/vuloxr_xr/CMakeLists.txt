subdirs(openxr_gl)

if(ANDROID)
  function(add_xr_sample TARGET_NAME)
    add_library(
      ${TARGET_NAME} SHARED
      #
      android_main.cpp ${ARGN})
    target_compile_definitions(
      ${TARGET_NAME} PRIVATE ANDROID=1 VK_USE_PLATFORM_ANDROID_KHR=1
                             -DXR_USE_PLATFORM_ANDROID)
    target_compile_options(
      ${TARGET_NAME} PUBLIC # DirectXMath
                            -Wno-defaulted-function-deleted)
    target_link_libraries(
      ${TARGET_NAME}
      PRIVATE vuloxr
              shaderc
              OpenXR::openxr_loader
              DirectXMath
              # android
              vulkan
              android
              log
              android_native_app_glue
              # for DirectXMath
              sal)

  endfunction()

else()
  function(add_xr_sample TARGET_NAME)
    add_executable(${TARGET_NAME} main.cpp ${ARGN})
    target_link_libraries(
      ${TARGET_NAME}
      PRIVATE vuloxr
              shaderc
              OpenXR::openxr_loader
              DirectXMath
              # windows
              Vulkan::Vulkan
              glfw
              #
              glew_s)
    target_compile_definitions(
      ${TARGET_NAME} PUBLIC -DXR_USE_PLATFORM_WIN32 -DWIN32_LEAN_AND_MEAN
                            -DNOMINMAX)

    if(MSVC)
      target_compile_definitions(${TARGET_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
      target_compile_options(${TARGET_NAME} PRIVATE /Zc:wchar_t /Zc:forScope
                                                    /W4 /EHsc)
    else()
      target_compile_options(${TARGET_NAME}
                             PRIVATE -Wno-defaulted-function-deleted)
    endif()

  endfunction()

endif()

add_xr_sample(hello_xr hello_xr/xr_main_loop.cpp hello_xr/CubeScene.cpp)
target_compile_definitions(hello_xr PUBLIC -DXR_USE_GRAPHICS_API_VULKAN)

add_xr_sample(gl2triOXR gl2triOXR/xr_main_loop.cpp)
target_compile_definitions(gl2triOXR PUBLIC -DXR_USE_GRAPHICS_API_VULKAN)

# set(TARGET_NAME gl2triOXR)
# if(ANDROID)
#
#   add_library(
#     ${TARGET_NAME} SHARED
#     #
#     android_main.cpp gl2triOXR/xr_main_loop.cpp)
#   target_compile_definitions(
#     ${TARGET_NAME} PRIVATE ANDROID=1 VK_USE_PLATFORM_ANDROID_KHR=1
#                            -DXR_USE_GRAPHICS_API_OPENGL_ES)
#   target_compile_options(${TARGET_NAME} PUBLIC # DirectXMath
#                                                -Wno-defaulted-function-deleted)
#   target_link_libraries(
#     ${TARGET_NAME} PRIVATE vuloxr openxr_gl OpenXR::openxr_loader
#                            android_native_app_glue DirectXMath sal)
# else()
#   add_executable(${TARGET_NAME} main.cpp gl2triOXR/xr_main_loop.cpp)
#
#   target_link_libraries(
#     ${TARGET_NAME}
#     PRIVATE vuloxr
#             openxr_gl
#             shaderc
#             OpenXR::openxr_loader
#             DirectXMath
#             # windows
#             Vulkan::Vulkan
#             glfw
#             #
#   )
#   target_compile_definitions(
#     ${TARGET_NAME} PUBLIC # -DWIN32_LEAN_AND_MEAN -DNOMINMAX
#                           -DXR_USE_GRAPHICS_API_OPENGL)
#   target_compile_definitions(${TARGET_NAME} PUBLIC)
#
#   if(MSVC)
#     target_compile_definitions(${TARGET_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
#     target_compile_options(${TARGET_NAME} PRIVATE /Zc:wchar_t /Zc:forScope /W4
#                                                   /EHsc)
#   else()
#     target_compile_options(${TARGET_NAME}
#                            PRIVATE -Wno-defaulted-function-deleted)
#   endif()
#
# endif()

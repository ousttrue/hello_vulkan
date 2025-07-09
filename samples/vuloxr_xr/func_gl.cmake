set(XR_SAMPLE_COMMON_LIBS vuloxr shaderc OpenXR::openxr_loader DirectXMath openxr_gl)

if(ANDROID)
  function(add_gl_sample TARGET_DIR)
    set(TARGET_NAME "${TARGET_DIR}_gl")
    add_library(
      ${TARGET_NAME} SHARED
      #
      android_main.cpp
      #
      ${TARGET_DIR}/xr_main_loop.cpp
      ${TARGET_DIR}/ViewRendererOpenGL.cpp
      ${ARGN})
    target_compile_definitions(
      ${TARGET_NAME}
      PRIVATE ANDROID=1 VK_USE_PLATFORM_ANDROID_KHR=1
              #
              -DXR_USE_PLATFORM_ANDROID -DXR_USE_GRAPHICS_API_OPENGL_ES)
    target_compile_options(
      ${TARGET_NAME} PUBLIC # DirectXMath
                            -Wno-defaulted-function-deleted)
    target_link_libraries(
      ${TARGET_NAME}
      PRIVATE ${XR_SAMPLE_COMMON_LIBS}
              # android
              android
              log
              android_native_app_glue
              # for DirectXMath
              sal)

  endfunction()

else()
  function(add_gl_sample TARGET_DIR)
    set(TARGET_NAME "${TARGET_DIR}_gl")
    add_executable(
      ${TARGET_NAME}
      #
      main.cpp
      #
      ${TARGET_DIR}/xr_main_loop.cpp
      ${TARGET_DIR}/ViewRendererOpenGL.cpp
      ${ARGN})
    target_link_libraries(
      ${TARGET_NAME}
      PRIVATE ${XR_SAMPLE_COMMON_LIBS}
              # windows
              glfw
              #
              GLEW::glew_s)
    target_compile_definitions(
      ${TARGET_NAME}
      PUBLIC -DXR_USE_PLATFORM_WIN32 -DXR_USE_GRAPHICS_API_OPENGL
             #
             -DWIN32_LEAN_AND_MEAN -DNOMINMAX)

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

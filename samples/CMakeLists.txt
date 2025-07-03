function(add_glsl TARGET SHADER)
  find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin)
  set(current-input-path ${CMAKE_CURRENT_LIST_DIR}/${SHADER})
  set(current-output-path ${CMAKE_CURRENT_BINARY_DIR}/${SHADER}.spv)
  add_custom_command(
    OUTPUT ${current-output-path}
    COMMAND ${GLSLC} -o ${current-output-path} ${current-input-path}
    DEPENDS ${current-input-path}
    IMPLICIT_DEPENDS CXX ${current-shader-path}
    VERBATIM)
  set_source_files_properties(${current-output-path} PROPERTIES GENERATED TRUE)
  target_sources(${TARGET} PRIVATE ${current-output-path})
endfunction(add_glsl)

subdirs(#
        hello_xr/cpp vuloxr)

if(ANDROID_NDK)
  # https://developer.android.com/ndk/guides/graphics/shader-compilers
  subdirs(ovr_openxr_mobile_sdk android_openxr_gles)
else()

endif()

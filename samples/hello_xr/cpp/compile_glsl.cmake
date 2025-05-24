# Find glslc shader compiler. On Android, the NDK includes the binary, so no
# external dependency.
if(ANDROID)
  file(GLOB glslc_folders CONFIGURE_DEPENDS ${ANDROID_NDK}/shader-tools/*)
  find_program(
    GLSL_COMPILER glslc
    PATHS ${glslc_folders}
    NO_DEFAULT_PATH)
else()
  if($ENV{VULKAN_SDK})
    file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" VULKAN_SDK)
    file(GLOB glslc_folders CONFIGURE_DEPENDS ${VULKAN_SDK}/*)
  endif()
  find_program(
    GLSL_COMPILER glslc
    PATHS ${glslc_folders}
    HINTS "${Vulkan_GLSLC_EXECUTABLE}")
endif()

find_program(GLSLANG_VALIDATOR glslangValidator
             HINTS "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}")
if(GLSL_COMPILER)
  message(STATUS "Found glslc: ${GLSL_COMPILER}")
elseif(GLSLANG_VALIDATOR)
  message(STATUS "Found glslangValidator: ${GLSLANG_VALIDATOR}")
else()
  message(STATUS "Could NOT find glslc, using precompiled .spv files")
endif()
function(compile_glsl run_target_name)
  set(glsl_output_files "")
  foreach(in_file IN LISTS ARGN)
    get_filename_component(glsl_stage "${in_file}" NAME_WE)
    set(out_file "${CMAKE_CURRENT_BINARY_DIR}/${glsl_stage}.spv")
    if(GLSL_COMPILER)
      # Run glslc if we can find it
      add_custom_command(
        OUTPUT "${out_file}"
        COMMAND "${GLSL_COMPILER}" -mfmt=c -fshader-stage=${glsl_stage}
                "${in_file}" -o "${out_file}"
        DEPENDS "${in_file}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
    elseif(GLSLANG_VALIDATOR)
      # Run glslangValidator if we can find it
      add_custom_command(
        OUTPUT "${out_file}"
        COMMAND "${GLSLANG_VALIDATOR}" -V -S ${glsl_stage} "${in_file}" -x -o
                "${out_file}"
        DEPENDS "${in_file}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
    else()
      # Use the precompiled .spv files
      get_filename_component(glsl_src_dir "${in_file}" DIRECTORY)
      set(precompiled_file "${glsl_src_dir}/${glsl_stage}.spv")
      configure_file("${precompiled_file}" "${out_file}" COPYONLY)
    endif()
    list(APPEND glsl_output_files "${out_file}")
  endforeach()
  add_custom_target(${run_target_name} ALL DEPENDS ${glsl_output_files})
  # set_target_properties(${run_target_name} PROPERTIES FOLDER ${HELPER_FOLDER})

endfunction()



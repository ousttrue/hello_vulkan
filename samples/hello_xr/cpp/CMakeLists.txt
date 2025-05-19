# Copyright (c) 2017-2024, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
set(CMAKE_CXX_STANDARD 20)

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

set(LOCAL_SOURCE
    # d3d_common.cpp
    # graphicsplugin_d3d11.cpp
    # graphicsplugin_d3d12.cpp
    # graphicsplugin_factory.cpp
    # graphicsplugin_opengl.cpp graphicsplugin_opengles.cpp
    graphicsplugin_vulkan.cpp
    # graphicsplugin_metal.cpp
    logger.cpp
    main.cpp
    openxr_program.cpp
    pch.cpp
    platformplugin_android.cpp
    # platformplugin_factory.cpp platformplugin_posix.cpp
    platformplugin_win32.cpp)
set(VULKAN_SHADERS vulkan_shaders/frag.glsl vulkan_shaders/vert.glsl)

if(ANDROID)
  add_library(hello_xr MODULE ${LOCAL_SOURCE} ${VULKAN_SHADERS}
                              $<TARGET_OBJECTS:android_native_app_glue>)
  target_compile_definitions(hello_xr PRIVATE -DXR_USE_PLATFORM_ANDROID
                                              -DXR_USE_GRAPHICS_API_VULKAN)
  target_link_libraries(
    hello_xr PRIVATE ${ANDROID_LIBRARY} ${ANDROID_LOG_LIBRARY}
                     android_native_app_glue vulkan)

  message(STATUS "hello_xr will default to Vulkan graphics plugin")
  target_compile_definitions(hello_xr PRIVATE DEFAULT_GRAPHICS_PLUGIN_VULKAN)

else()
  add_executable(hello_xr ${LOCAL_SOURCE} ${VULKAN_SHADERS})
  target_compile_definitions(
    hello_xr PRIVATE -DXR_USE_PLATFORM_WIN32 -DXR_USE_GRAPHICS_API_VULKAN,
                     -DWIN32_LEAN_AND_MEAN -DNOMINMAX)

  target_link_libraries(hello_xr PRIVATE vulkan_sdk)
endif()
target_include_directories(hello_xr PRIVATE .)

if(APPLE)
  # use C++17 since there is a dependency on metal-cpp
  target_compile_features(hello_xr PUBLIC cxx_std_17)
endif()

# set_target_properties(hello_xr PROPERTIES FOLDER ${SAMPLES_FOLDER})
source_group("Shaders" FILES ${VULKAN_SHADERS})

target_link_libraries(hello_xr PRIVATE OpenXR::openxr_loader)

compile_glsl(run_hello_xr_glsl_compiles ${VULKAN_SHADERS})

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

target_link_libraries(hello_xr PRIVATE OpenXR::openxr_loader)

if(GLSLANG_VALIDATOR AND NOT GLSLC_COMMAND)
  target_compile_definitions(hello_xr PRIVATE USE_GLSLANGVALIDATOR)
endif()

if(XR_USE_GRAPHICS_API_VULKAN)
  target_include_directories(hello_xr PRIVATE ${Vulkan_INCLUDE_DIRS})
  target_link_libraries(hello_xr PRIVATE ${Vulkan_LIBRARY})
endif()

if(TARGET openxr-gfxwrapper)
  target_link_libraries(hello_xr PRIVATE openxr-gfxwrapper)
endif()
if(WIN32)
  target_link_libraries(hello_xr PRIVATE ole32)
  if(MSVC)
    target_compile_definitions(hello_xr PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(hello_xr PRIVATE /Zc:wchar_t /Zc:forScope /W4)
    # if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" ) # If actually msvc and not
    # clang-cl target_compile_options(openxr_c_compile_test PRIVATE /WX) endif()

    # Right now can't build d3d features on MinGW because of directxcolors,
    # directxmath, etc.
    target_link_libraries(hello_xr PRIVATE d3d11 d3d12 d3dcompiler dxgi)
  endif()
endif()

if(APPLE)
  target_link_libraries(
    hello_xr PRIVATE "-framework Foundation" "-framework CoreGraphics"
                     "-framework Metal")
endif()

if(NOT ANDROID)
  install(TARGETS hello_xr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                                   COMPONENT hello_xr)
  if(NOT WIN32)
    install(
      FILES hello_xr.1
      DESTINATION ${CMAKE_INSTALL_MANDIR}/man1/
      COMPONENT ManPages)
  endif()
endif()

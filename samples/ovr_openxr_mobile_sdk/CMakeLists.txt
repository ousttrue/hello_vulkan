set(CMAKE_CXX_STANDARD 20)

add_library(meta_openxr_preview INTERFACE)
target_include_directories(meta_openxr_preview
                           INTERFACE ${CMAKE_CURRENT_LIST_DIR}/OpenXR)

if(ANDROID)
  # use prebuilt
else()

  if(MSVC)
    add_compile_options(/Wv:18 /EHsc)
    add_definitions(
      -D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
      -D_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS -D_USE_MATH_DEFINES)
  endif()

endif()

subdirs(
  3rdParty
  1stParty
  SampleXrFramework
  XrHandsFB/cpp
  XrPassthrough/cpp
  XrVirtualKeyboard/cpp
  XrHandsAndControllers/cpp)

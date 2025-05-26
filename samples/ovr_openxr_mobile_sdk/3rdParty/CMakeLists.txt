add_library(glext INTERFACE)
target_include_directories(glext INTERFACE ${CMAKE_CURRENT_LIST_DIR}/glext)

add_library(xr_linear INTERFACE)
target_include_directories(
  xr_linear
  INTERFACE ${CMAKE_CURRENT_LIST_DIR}/khronos/openxr/OpenXR-SDK/src/common)

subdirs(minizip)

if(ANDROD)

else()
  set(BUILD_SHARED_LIBS OFF)
  set(KTX_FEATURE_STATIC_LIBRARY ON)
  FetchContent_Declare(
    libktx
    GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
    GIT_TAG v4.4.0)
  FetchContent_MakeAvailable(libktx)
endif()

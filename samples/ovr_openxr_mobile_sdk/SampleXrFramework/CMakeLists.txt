file(GLOB_RECURSE CPP_SOURCES "Src/*.cpp")
file(GLOB_RECURSE C_SOURCES "Src/*.c")
add_library(samplexrframework STATIC ${CPP_SOURCES} ${C_SOURCES})
target_include_directories(samplexrframework
                           PUBLIC ${CMAKE_CURRENT_LIST_DIR}/Src)
target_link_libraries(
  samplexrframework
  PRIVATE minizip zlib stb ktx
  PUBLIC OVR glext OpenXR::openxr_loader meta_openxr_preview xr_linear)

if(ANDROID)

else()
  target_compile_definitions(samplexrframework PUBLIC WIN32 KHRONOS_STATIC)
  if(MSVC)
    target_compile_definitions(samplexrframework PRIVATE NOMINMAX)
    target_link_libraries(samplexrframework PUBLIC OpenGL32)
  endif()
endif()

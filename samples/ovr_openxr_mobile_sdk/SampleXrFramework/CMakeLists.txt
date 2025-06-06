file(GLOB_RECURSE CPP_SOURCES "Src/*.cpp")
file(GLOB_RECURSE C_SOURCES "Src/*.c")
add_library(samplexrframework STATIC ${CPP_SOURCES} ${C_SOURCES})
target_include_directories(samplexrframework
                           PUBLIC ${CMAKE_CURRENT_LIST_DIR}/Src)
target_link_libraries(
  samplexrframework
  PRIVATE stb ktx
  PUBLIC OVR minizip glext OpenXR::openxr_loader meta_openxr_preview xr_linear)

if(ANDROID)
  target_link_libraries(samplexrframework PUBLIC android_native_app_glue
                                                 android EGL GLESv3 log)
else()
  target_compile_definitions(samplexrframework PUBLIC WIN32 KHRONOS_STATIC)
  if(MSVC)
    target_compile_definitions(samplexrframework PRIVATE NOMINMAX)
    target_link_libraries(samplexrframework PUBLIC OpenGL32)
  endif()
endif()

add_library(common STATIC logger.cpp)
target_include_directories(common PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

if(ANDROID)
  target_compile_definitions(common PUBLIC -DXR_USE_PLATFORM_ANDROID
                                           -DXR_USE_GRAPHICS_API_VULKAN)

  target_link_libraries(common PUBLIC ${ANDROID_LIBRARY} ${ANDROID_LOG_LIBRARY}
                                      android_native_app_glue)
else()
  target_compile_definitions(
    common PUBLIC -DXR_USE_PLATFORM_WIN32 -DXR_USE_GRAPHICS_API_VULKAN
                  -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
endif()

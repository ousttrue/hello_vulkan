set(CMAKE_CXX_STANDARD 20)

add_library(vko INTERFACE)
target_include_directories(vko INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

if(ANDROID)
  target_link_libraries(vko INTERFACE vulkan)

  set(TARGET_NAME 01_clear)
  add_library(
    ${TARGET_NAME} SHARED
    #
    android_main.cpp 01_clear/main_loop.cpp #
  )
  target_compile_definitions(${TARGET_NAME}
                             PRIVATE ANDROID=1 VK_USE_PLATFORM_ANDROID_KHR=1)
  target_link_libraries(${TARGET_NAME} PRIVATE android_native_app_glue android
                                               log vko shaderc)

else()
  target_link_libraries(vko INTERFACE Vulkan::Vulkan)

  add_executable(
    01_clear
    #
    main.cpp 01_clear/main_loop.cpp #
  )
  target_link_libraries(01_clear PRIVATE vko glfw)

endif()

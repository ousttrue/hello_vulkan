set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME vulfwk)

if(ANDROID_NDK)
  add_library(${TARGET_NAME} SHARED android_main.cpp vulfwk_pipeline.cpp
                                    main_loop.cpp)
  target_compile_definitions(${TARGET_NAME}
                             PRIVATE ANDROID=1 VK_USE_PLATFORM_ANDROID_KHR=1)
  target_link_libraries(${TARGET_NAME} PRIVATE android_native_app_glue android
                                               log vko)
else()
  add_executable(${TARGET_NAME} main.cpp vulfwk_pipeline.cpp main_loop.cpp
                                glsl_to_spv.cpp)
  target_compile_definitions(${TARGET_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR=1)
  target_link_libraries(
    ${TARGET_NAME}
    PRIVATE vko glfw
            #
            Vulkan::shaderc_combined)
  target_compile_options(${TARGET_NAME} PRIVATE /EHsc)
endif()

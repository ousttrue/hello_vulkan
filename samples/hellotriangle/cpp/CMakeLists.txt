set(TARGET native)
add_library(
  ${TARGET} SHARED
  main.cpp
  pipeline.cpp
  main_loop.cpp
  #
  ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c)
target_compile_definitions(${TARGET} PRIVATE VK_USE_PLATFORM_ANDROID_KHR=1)
target_include_directories(
  ${TARGET} PRIVATE ${ANDROID_NDK}/sources/android/cpufeatures
                    ${magic_enum_SOURCE_DIR}/include)
target_link_libraries(${TARGET} PRIVATE android log android_native_app_glue
                                        vko)

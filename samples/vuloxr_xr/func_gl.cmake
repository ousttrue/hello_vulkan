
#
# OPENGL
#
# if(ANDROID)
#
# set(TARGET_NAME gl2triOXR) if(ANDROID)
#
# add_library( ${TARGET_NAME} SHARED # android_main.cpp
# gl2triOXR/xr_main_loop.cpp gl2triOXR/ViewRendererOpenGL.cpp)
# target_compile_definitions( ${TARGET_NAME} PRIVATE ANDROID=1
# VK_USE_PLATFORM_ANDROID_KHR=1 -DXR_USE_GRAPHICS_API_OPENGL_ES)
# target_compile_options(${TARGET_NAME} PUBLIC # DirectXMath
# -Wno-defaulted-function-deleted) target_link_libraries( ${TARGET_NAME} PRIVATE
# vuloxr openxr_gl OpenXR::openxr_loader android_native_app_glue DirectXMath
# sal) else() add_executable(${TARGET_NAME} main.cpp gl2triOXR/xr_main_loop.cpp
# gl2triOXR/ViewRendererOpenGL.cpp)
#
# target_link_libraries( ${TARGET_NAME} PRIVATE vuloxr openxr_gl shaderc
# OpenXR::openxr_loader DirectXMath # windows Vulkan::Vulkan glfw # )
# target_compile_definitions( ${TARGET_NAME} PUBLIC # -DWIN32_LEAN_AND_MEAN
# -DNOMINMAX -DXR_USE_GRAPHICS_API_OPENGL)
# target_compile_definitions(${TARGET_NAME} PUBLIC)
#
# if(MSVC) target_compile_definitions(${TARGET_NAME} PRIVATE
# _CRT_SECURE_NO_WARNINGS) target_compile_options(${TARGET_NAME} PRIVATE
# /Zc:wchar_t /Zc:forScope /W4 /EHsc) else()
# target_compile_options(${TARGET_NAME} PRIVATE -Wno-defaulted-function-deleted)
# endif()
#
# endif()

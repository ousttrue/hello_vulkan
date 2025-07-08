cmake_minimum_required(VERSION 3.20)
project(openxr-example)

# for compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# for build/.gitignore
if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  file(
    GENERATE
    OUTPUT .gitignore
    CONTENT "*")
endif()

set(TARGET_NAME openxr-example)
add_executable(${TARGET_NAME} main.cpp glimpl.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE SDL2-static glew_s
                                             OpenXR::openxr_loader)

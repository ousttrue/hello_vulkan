set(TARGET_NAME vulkan_tutorial)

FetchContent_Declare(
  glfw
  URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
)
FetchContent_MakeAvailable(glfw)

function(add_shader TARGET SHADER)
  find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Include/Bin)
  set(current-input-path ${CMAKE_CURRENT_LIST_DIR}/${SHADER})
  set(current-output-path ${CMAKE_CURRENT_BINARY_DIR}/${SHADER}.spv)
  add_custom_command(
    OUTPUT ${current-output-path}
    COMMAND ${GLSLC} -o ${current-output-path} ${current-input-path}
    DEPENDS ${current-input-path}
    IMPLICIT_DEPENDS CXX ${current-shader-path}
    VERBATIM)
  set_source_files_properties(${current-output-path} PROPERTIES GENERATED TRUE)
  target_sources(${TARGET} PRIVATE ${current-output-path})
endfunction(add_shader)

SET(CMAKE_CXX_STANDARD 17)
add_executable(${TARGET_NAME}
  main.cpp
)
target_include_directories(${TARGET_NAME} PRIVATE
  $ENV{VULKAN_SDK}/Include
  ${glm_SOURCE_DIR}
)
target_link_directories(${TARGET_NAME} PRIVATE
  $ENV{VULKAN_SDK}/Lib
)
target_link_libraries(${TARGET_NAME} PRIVATE 
  glfw
  vulkan-1.lib
)
add_shader(${TARGET_NAME} shader.vert)
add_shader(${TARGET_NAME} shader.frag)

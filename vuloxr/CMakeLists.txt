FetchContent_Populate(
  imgui URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b.zip)
set(TARGET_NAME imgui)
add_library(
  ${TARGET_NAME} STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
target_include_directories(${TARGET_NAME} PUBLIC ${imgui_SOURCE_DIR}
                                                 ${imgui_SOURCE_DIR}/backends)
target_link_libraries(${TARGET_NAME} PUBLIC Vulkan::Vulkan glfw)

FetchContent_Declare(
  jsoncpp
  URL https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.9.6.zip
)
FetchContent_MakeAvailable(jsoncpp)

set(TARGET_NAME vuloxr_sample)
add_executable(${TARGET_NAME} main.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE imgui jsoncpp_static)

file(GLOB sources *.cpp)
file(GLOB sources-headers *.hpp)
file(GLOB device-sources device/*.cpp)
file(GLOB device-sources-headers device/*.hpp)
add_library(framework STATIC
	${sources}
	${sources-headers}
	${device-sources}
	${device-sources-headers})

target_include_directories(framework 
PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR}/.. 
  ${CMAKE_CURRENT_SOURCE_DIR} 
  ${glm_SOURCE_DIR}
  ${stb_SOURCE_DIR}
PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(framework vulkan)


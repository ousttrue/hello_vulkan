# dependencies
set(BUILD_SHARED_LIBS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)

FetchContent_Populate(
  glm
  URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip
  # URL https://github.com/g-truc/glm/releases/download/1.0.1/glm-1.0.1-light.zip
)

FetchContent_Populate(
  stb
  URL https://github.com/nothings/stb/archive/refs/heads/master.zip
)

if(${PLATFORM} MATCHES "android")
  subdirs(
    hellotriangle
)
else()
  subdirs(
    vulkan_tutorial
  )
endif()


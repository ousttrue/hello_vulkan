add_executable(xrpassthrough XrPassthrough.cpp XrPassthroughGL.cpp
                             XrPassthroughInput.cpp)
target_link_libraries(xrpassthrough samplexrframework)

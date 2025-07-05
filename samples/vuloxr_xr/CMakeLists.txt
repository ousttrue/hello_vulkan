subdirs(openxr_gl)

include(func_vk.cmake)
include(func_gl.cmake)

add_vk_sample(hello_xr)
add_vk_sample(gl2triOXR)
add_gl_sample(hello_xr)
add_gl_sample(gl2triOXR)


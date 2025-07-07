glew_dep = dependency('glew', default_options: ['default_library=static'])
grapho_dep = dependency('grapho')
directxmath_dep = dependency('directxmath')
cuber_srcs = [
    'src/mesh.cpp',
    'src/gl3/GlCubeRenderer.cpp',
    'src/gl3/GlLineRenderer.cpp',
]
if host_machine.system() == 'windows' and get_option('d3d')
    cuber_srcs += [
        'src/dx/DxCubeRenderer.cpp',
        'src/dx/DxCubeRendererImpl.cpp',
        'src/dx/DxCubeStereoRenderer.cpp',
        'src/dx/DxLineRenderer.cpp',
    ]
endif
cuber_inc = include_directories('include')
cuber_lib = static_library(
    'cuber',
    cuber_srcs,
    include_directories: cuber_inc,
    dependencies: [glew_dep, grapho_dep, directxmath_dep],
)
cuber_dep = declare_dependency(
    link_with: cuber_lib,
    include_directories: cuber_inc,
    dependencies: [glew_dep, grapho_dep],
)

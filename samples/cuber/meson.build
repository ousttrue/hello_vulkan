directxmath_dep = dependency('directxmath')
asio_dep = dependency('asio')
meshutils_dep = dependency('meshutils')

bvhutil_lib = static_library(
    'bvhutil',
    [
        'GuiApp.cpp',
        'Bvh.cpp',
        'BvhSolver.cpp',
        'BvhNode.cpp',
        'Animation.cpp',
        'UdpSender.cpp',
        'BvhPanel.cpp',
        'Payload.cpp',
        'BvhFrame.cpp',
    ],
    dependencies: [
        imgui_dep,
        directxmath_dep,
        asio_dep,
        meshutils_dep,
        cuber_dep,
    ],
)
bvhutil_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: bvhutil_lib,
    dependencies: [imgui_dep, directxmath_dep],
)

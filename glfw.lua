project "glfw3"

local f=io.open("glfw/src/glfw_config.h", "w")
f:write [[
#define _GLFW_WIN32
#define _GLFW_BUILD_DLL
]]
f:close()

--kind "ConsoleApp"
--kind "WindowedApp"
--kind "StaticLib"
kind "SharedLib"
language "C"
--language "C++"

objdir "%{prj.name}"

flags{
    --"WinMain"
    --"Unicode",
    --"StaticRuntime",
}
files {
    "glfw/src/internal.h",
    "glfw/src/glfw_config.h",
    "glfw/include/GLFW/glfw3.h",
    "glfw/include/GLFW/glfw3native.h",
    "glfw/src/win32_platform.h",
    "glfw/src/win32_joystick.h",
    "glfw/src/wgl_context.h",
    "glfw/src/egl_context.h",
    "glfw/src/context.c",
    "glfw/src/init.c",
    "glfw/src/input.c",
    "glfw/src/monitor.c",
    "glfw/src/vulkan.c",
    "glfw/src/window.c",
    "glfw/src/win32_init.c",
    "glfw/src/win32_joystick.c",
    "glfw/src/win32_monitor.c",
    "glfw/src/win32_time.c",
    "glfw/src/win32_tls.c",
    "glfw/src/win32_window.c",
    "glfw/src/wgl_context.c",
    "glfw/src/egl_context.c",
}
includedirs { 
    "glfw/src",
}
defines { 
    "WIN32",
    "_WINDOWS",
    "_GLFW_USE_CONFIG_H",
    "glfw_EXPORTS",
}
buildoptions { }
libdirs { }
links { }

postbuildcommands{
    "copy $(TargetDir)$(TargetName).dll ..\\..",
}
filter { "configurations:Debug" }
    postbuildcommands{
        "copy $(TargetDir)$(TargetName).pdb ..\\..",
    }


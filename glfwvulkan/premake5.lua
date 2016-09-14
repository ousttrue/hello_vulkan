project "glfwvulkan"

kind "ConsoleApp"
--kind "WindowedApp"
--kind "StaticLib"
--kind "SharedLib"
--language "C"
language "C++"

objdir "%{prj.name}"

flags{
    --"WinMain"
    --"Unicode",
    --"StaticRuntime",
}
files {
    "*.cpp",
    "*.h",
}
includedirs { 
    "../glfw/include",
}
defines { 
    "WIN32",
    "_WINDOWS",
    "GLFW_DLL",
}
buildoptions { }
libdirs { }
links { 
    "glfw3",
}


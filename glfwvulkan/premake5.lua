project "glfwvulkan"

--kind "ConsoleApp"
kind "WindowedApp"
--kind "StaticLib"
--kind "SharedLib"
--language "C"
language "C++"

objdir "%{prj.name}"

flags{
    "WinMain"
    --"Unicode",
    --"StaticRuntime",
}
files {
    "*.cpp",
    "*.h",
}
includedirs { 
    "../glfw/include",
    "C:/VulkanSDK/1.0.26.0/Include",
}
defines { 
    "WIN32",
    "_WINDOWS",
    "GLFW_DLL",
    "VK_USE_PLATFORM_WIN32_KHR",
}
buildoptions { }
libdirs { 
    "C:/VulkanSDK/1.0.26.0/Bin",
}
links { 
    "glfw3",
    "vulkan-1",
}


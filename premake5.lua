local build_dir="_build_premake"

-- premake5.lua
location(build_dir)

solution "glfwvulkan"
do
    configurations { "Debug", "Release" }
    platforms { "Win64" }
end

filter "configurations:Debug"
do
    defines { "DEBUG", "_DEBUG" }
    flags { "Symbols" }
end

filter "configurations:Release"
do
    defines { "NDEBUG" }
    optimize "On"
end

filter { "platforms:Win64" }
   architecture "x64"
filter {"platforms:Win64", "configurations:Debug" }
    targetdir(build_dir.."/Win64_Debug")
filter {"platforms:Win64", "configurations:Release" }
    targetdir(build_dir.."/Win64_Release")

filter { "action:vs*" }
    buildoptions {
        "/wd4996",
    }
    defines {
        "_CRT_SECURE_NO_DEPRECATE",
    }
    characterset ("MBCS")

filter {}

include "glfwvulkan"
dofile "glfw.lua"


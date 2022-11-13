-- A Visual studio solution.
workspace "NetherEngine"
    configurations
    {
        "Debug",
        "Release"
    }
    architecture "x64"

-- A Visual studio project.
project "NetherEngine"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    objdir "bin-int/%{cfg.buildcfg}"
    targetdir "bin/%{cfg.buildcfg}"

    files
    {
        "src/**.cpp",
        "include/**.hpp",
        "shaders/**.hlsl"
    }

    includedirs 
    { 
        "include/",
        "include/NetherEngine",
    }

    links
    {
        "d3d12",
        "dxgi",
        "dxguid",
        "dxcompiler",
    }

    pchheader "Pch.hpp"
    pchsource "src/Pch.cpp"

    staticruntime "Off"

    filter "files:**.hlsl"
        buildaction "None"

    filter "configurations:Debug"
        defines "NETHER_DEBUG"
        symbols "On"
        optimize "Debug"

    filter "configurations:Release"
        defines "NETHER_RELEASE"
        optimize "Speed"

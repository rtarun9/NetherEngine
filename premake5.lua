workspace "NetherEngine"
    configurations
    {
        "Debug",
        "Release"
    }

project "NetherEngine"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    objdir "Output-Int/%{cfg.buildcfg}"
    targetdir "Output/%{cfg.buildcfg}"

    files
    {
        "NetherEngine/**.cpp",
        "NetherEngine/**.hpp"
    }

    includedirs 
    { 
        "NetherEngine/" 
    }

    links
    {
        "d3d12.lib",
        "dxgi.lib",
        "dxguid.lib",
        "dxcompiler.lib"
    }

    pchheader "Pch.hpp"
    pchsource "NetherEngine/Pch.cpp"

    filter "configurations:Debug"
        defines "NETHER_DEBUG"
        symbols "On"
        optimize "Debug"

    filter "configurations:Release"
        defines "NETHER_RELEASE"
        optimize "Speed"

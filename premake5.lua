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

    objdir "Output/%{cfg.buildcfg}"
    targetdir "Output-Int/%{cfg.buildcfg}"

    files
    {
        "NetherEngine/**.cpp",
        "NetherEngine/**.hpp"
    }

    includedirs 
    { 
        "NetherEngine/" 
    }

    filter "configurations:Debug"
        defines "NETHER_DEBUG"
        symbols "On"
        optimize "Debug"

    filter "configurations:Release"
        defines "NETHER_RELEASE"
        optimize "Speed"

    pchheader "Pch.hpp"
    pchsource "NetherEngine/Pch.cpp"
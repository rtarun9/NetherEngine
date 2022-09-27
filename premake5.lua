workspace "NetherEngine"
    configurations
    {
        "Debug",
        "Release"
    }
    architecture "x64"

function IncludeAndLinkThirdParty()
    includedirs "ThirdParty/DirectXShaderCompiler/inc/"
    libdirs "ThirdParty/DirectXShaderCompiler/bin/x64/"
end

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
        "NetherEngine/",
    }

    links
    {
        "d3d12",
        "dxgi",
        "dxguid",
        "dxcompiler",
    }

    IncludeAndLinkThirdParty()

    pchheader "Pch.hpp"
    pchsource "NetherEngine/Pch.cpp"

    staticruntime "Off"

    filter "configurations:Debug"
        defines "NETHER_DEBUG"
        symbols "On"
        optimize "Debug"

    filter "configurations:Release"
        defines "NETHER_RELEASE"
        optimize "Speed"

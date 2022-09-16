set(SRC_FILES
    "Source/NetherEngine/Main.cpp"
    "Source/NetherEngine/Application.cpp"
    "Source/NetherEngine/Engine.cpp"
    "Source/NetherEngine/Utils.cpp")

add_executable(NetherEngine WIN32 ${SRC_FILES})

target_precompile_headers(NetherEngine PUBLIC Source/Pch.hpp)
target_link_libraries(NetherEngine PUBLIC d3d12.lib dxgi.lib dxguid.lib)
target_include_directories(NetherEngine PUBLIC "Source")

find_path(D3DX12_INCLUDE_DIRS "d3dx12.h")
target_include_directories(NetherEngine PRIVATE ${D3DX12_INCLUDE_DIRS})
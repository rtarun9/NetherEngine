set(SRC_FILES
	"Source/Common/DataTypes.hpp"
	"Source/Common/Utils.hpp"
	"Source/Common/Utils.cpp"
	"Source/Core/Application.hpp"
	"Source/Core/Application.cpp"
	"Source/Core/Engine.hpp"
	"Source/Core/Engine.cpp"
	"Source/Core/Main.cpp"
	"Source/Loaders/ModelLoader.hpp"
	"Source/Loaders/ModelLoader.cpp"
	"Source/Loaders/TextureLoader.hpp"
	"Source/Loaders/TextureLoader.cpp"
	"Source/Rendering/CommandQueue.hpp"
	"Source/Rendering/CommandQueue.cpp"
	"Source/Rendering/DescriptorHeap.hpp"
	"Source/Rendering/DescriptorHeap.cpp"
	"Source/Rendering/Device.hpp"
	"Source/Rendering/Device.cpp"
	"Source/Rendering/GraphicsCommon.hpp"
	"Source/Rendering/GraphicsCommon.cpp"
	"Source/Rendering/Resources.hpp"
	"Source/Rendering/Resources.cpp"
	"Source/Rendering/Shader.hpp"
	"Source/Rendering/Shader.cpp"
	"Source/Scene/Camera.hpp"
	"Source/Scene/Camera.cpp"
	"Source/Scene/GameObject.hpp"
	"Source/Scene/GameObject.cpp")

set(PCHHeader
	"Source/Pch.hpp")

add_executable(NetherEngine WIN32 ${SRC_FILES})
target_precompile_headers(NetherEngine PUBLIC ${PCHHeader})

find_path(D3DX12_INCLUDE_DIRS "d3dx12.h")
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
find_package(imgui CONFIG REQUIRED)

target_include_directories(NetherEngine PUBLIC "Source")
target_include_directories(NetherEngine PRIVATE ${D3DX12_INCLUDE_DIRS} ${TINYGLTF_INCLUDE_DIRS})

target_link_libraries(NetherEngine PUBLIC d3d12.lib dxgi.lib dxguid.lib dxcompiler.lib)
target_link_libraries(NetherEngine PRIVATE imgui::imgui)

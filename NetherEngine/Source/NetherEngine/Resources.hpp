#pragma once

namespace nether
{
	// Required because of const char *values in the semantic name of the input element desc's, as it is constantly null when Shader object is returned from the Compile function.
	struct InputElementDesc
	{
		std::string semanticName{};
		D3D12_INPUT_ELEMENT_DESC inputElementDesc{};
	};
	
	struct VertexBufferCreationDesc
	{
		uint32_t numberOfElements{};
		uint32_t stride{};
	};

	struct VertexBuffer
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> resource{};
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	};

	struct IndexBufferCreationDesc
	{
		uint32_t bufferSize{};
		uint32_t indicesCount{};
		DXGI_FORMAT format{};
	};

	struct IndexBuffer
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> resource{};
		uint32_t indicesCount{};
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	};

	struct ConstantBufferCreationDesc
	{
		size_t bufferSize{};
	};

	struct ConstantBuffer
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> resource{};
		uint64_t bufferSize{};
		uint8_t* mappedBufferPointer{};

		void Update(void* data);
	};
}
struct RenderResources
{
	uint positionBufferIndex;
	uint colorBufferIndex;
};

struct TransformBuffer
{
	row_major matrix modelMatrix;
	row_major matrix viewProjectionMatrix;
};

ConstantBuffer<RenderResources> renderResources : register(b0);
ConstantBuffer<TransformBuffer> transformBuffer : register(b1);

struct VertexOutput
{
	float4 position : SV_Position;
	float4 color : COLOR;
};

VertexOutput VsMain(uint vertexID : SV_VertexID)
{
	StructuredBuffer<float3> positionBuffer = ResourceDescriptorHeap[renderResources.positionBufferIndex];
	StructuredBuffer<float3> colorBuffer = ResourceDescriptorHeap[renderResources.colorBufferIndex];

	VertexOutput output;
	output.position = mul(float4(positionBuffer[vertexID], 1.0f), mul(transformBuffer.modelMatrix, transformBuffer.viewProjectionMatrix));
	output.color = float4(colorBuffer[vertexID], 1.0f);

	return output;
}

float4 PsMain(VertexOutput input) : SV_Target
{
	const float3 color = pow(input.color.rgb, 1 / 2.2f);
    return float4(color, 1.0f);
}


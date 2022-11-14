struct VertexInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color : COLOR;
};

struct TransformData { row_major matrix mvpMatrix; };

ConstantBuffer<TransformData> transformBuffer : register(b0);

VertexOutput VsMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(float4(input.position, 1.0f), transformBuffer.mvpMatrix);
    output.color = float4(input.color, 1.0f);

    return output;
}

float4 PsMain(VertexOutput input) : SV_Target
{
    input.color.rgb = pow(input.color.rgb, 1 / 2.2f);
    return input.color;
}
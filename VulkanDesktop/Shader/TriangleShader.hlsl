struct VSInput {
    float3 position : SV_POSITION;
    float3 color    : COLOR;
    float2 texCoord : TEXCOORD;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float2 texCoord : TEXCOORD;
};

cbuffer CameraData : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

// Vertex Shader
PSInput VSMain(VSInput input) {
    PSInput output;

    output.position = mul(float4(input.position, 1.f), proj * view * model);
    output.color = float4(input.color, 1.f);
    output.texCoord = input.texCoord;

    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    //return input.color;
    return float4(1.f, 0.f, 1.f, 1.f);
}
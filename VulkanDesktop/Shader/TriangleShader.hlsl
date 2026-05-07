struct VSInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 color    : COLOR0;
    [[vk::location(2)]] float2 texCoord : TEXCOORD0;
};

struct PSInput {
    float4 position                  : SV_POSITION;
    [[vk::location(0)]] float4 color : COLOR0;
    [[vk::location(1)]] float2 texCoord : TEXCOORD0;
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

    // Keep the same transform order as the previous GLSL shader.
    output.position = mul(proj * view * model, float4(input.position, 1.f));
    output.color = float4(input.color, 1.f);
    output.texCoord = input.texCoord;

    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    //return input.color;
    return float4(1.f, 0.f, 1.f, 1.f);
}
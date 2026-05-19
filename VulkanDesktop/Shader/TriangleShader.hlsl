struct VSInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 color    : COLOR0;
    [[vk::location(2)]] float2 texCoord : TEXCOORD0;
    [[vk::location(3)]] float3 normal   : NORMAL;
};

struct PSInput {
    float4 position                       : SV_POSITION;
    [[vk::location(0)]] float3 color      : COLOR0;
    [[vk::location(1)]] float2 texCoord   : TEXCOORD0;
    [[vk::location(2)]] float3 worldNormal : TEXCOORD1;
    [[vk::location(3)]] float3 worldPos   : TEXCOORD2;
};

cbuffer CameraData : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

cbuffer EnvironmentData : register(b1)
{
    float4 fogColor;
    float4 fogDistances;
    float4 ambientColor;
    float4 sunlightDirection;
    float4 sunlightColor;
    float4 viewWorldPos;
};

Texture2D gAlbedoMap : register(t2);
SamplerState gAlbedoSampler : register(s2);

PSInput VSMain(VSInput input) {
    PSInput output;

    const float4 worldPosition = mul(model, float4(input.position, 1.f));
    output.position = mul(proj * view, worldPosition);
    output.color = input.color;
    output.texCoord = input.texCoord;
    output.worldNormal = normalize(mul((float3x3)model, input.normal));
    output.worldPos = worldPosition.xyz;

    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    const float specularStrength = fogDistances.x;
    const float shininess = max(fogDistances.y, 1.0);
    const float textureBlend = saturate(fogDistances.z);

    const float3 texAlbedo = gAlbedoMap.Sample(gAlbedoSampler, input.texCoord).rgb;
    const float3 albedo = lerp(input.color, texAlbedo, textureBlend);

    const float3 N = normalize(input.worldNormal);
    const float3 L = normalize(sunlightDirection.xyz);
    const float3 V = normalize(viewWorldPos.xyz - input.worldPos);
    const float3 H = normalize(L + V);

    const float3 ambient = ambientColor.rgb * albedo;
    const float ndotl = saturate(dot(N, L));
    const float3 diffuse = sunlightColor.rgb * albedo * ndotl;

    const float spec = specularStrength * pow(saturate(dot(N, H)), shininess);
    const float3 specular = sunlightColor.rgb * spec;

    return float4(ambient + diffuse + specular, 1.f);
}

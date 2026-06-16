#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneHdr;
layout(set = 0, binding = 1) uniform sampler2D bloomTex;

layout(push_constant) uniform PushConstants {
    float exposure;
    float bloomIntensity;
    uint tonemapEnabled;
    uint bloomEnabled;
    uint tonemapMode;
} pc;

vec3 TonemapReinhard(vec3 hdr)
{
    return hdr / (hdr + vec3(1.0));
}

vec3 TonemapAces(vec3 hdr)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((hdr * (a * hdr + b)) / (hdr * (c * hdr + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 color = texture(sceneHdr, vUV).rgb * pc.exposure;
    if (pc.bloomEnabled != 0u) {
        color += texture(bloomTex, vUV).rgb * pc.bloomIntensity;
    }

    if (pc.tonemapEnabled != 0u) {
        color = (pc.tonemapMode != 0u) ? TonemapAces(color) : TonemapReinhard(color);
    }

    outColor = vec4(color, 1.0);
}

#version 450

layout(push_constant) uniform ShadowPush {
    mat4 lightViewProj;
    vec4 biasParams;  // x = normalBias (world-space units)
} pc;

layout(set = 0, binding = 0) uniform ObjectData {
    mat4 model;
    uint materialIndex;
} objectData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

void main()
{
    vec4 worldPosition = objectData.model * vec4(inPosition, 1.0);
    // Offset along world-space vertex normal to prevent shadow acne.
    // Applied in world space to avoid object-scale dependency (e.g. Sponza 0.01 scale).
    vec3 worldNormal = normalize(mat3(objectData.model) * inNormal);
    worldPosition.xyz += worldNormal * pc.biasParams.x;
    gl_Position = pc.lightViewProj * worldPosition;
}

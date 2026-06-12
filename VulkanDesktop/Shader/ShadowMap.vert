#version 450

layout(push_constant) uniform ShadowPush {
    mat4 lightViewProj;
} pc;

layout(set = 2, binding = 0) uniform ObjectData {
    mat4 model;
    uint materialIndex;
} objectData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

void main()
{
    const vec4 worldPosition = objectData.model * vec4(inPosition, 1.0);
    gl_Position = pc.lightViewProj * worldPosition;
}

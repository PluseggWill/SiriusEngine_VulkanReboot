#version 450

// Same bindings as TriangleVertex.vert — G-buffer geometry pass (slice 1).
// CameraData is VERTEX-stage only — pass clip positions; frag does perspective-correct UV/MV.
layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 proj;
    mat4 prevViewProj;
    mat4 currViewProj;
    vec4 temporalJitterAndFlags;
} cameraData;

layout(set = 2, binding = 0) uniform ObjectData {
    mat4 model;
    uint materialIndex;
} objectData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outWorldNormal;
layout(location = 3) out vec3 outWorldPos;
layout(location = 4) flat out uint outMaterialIndex;
layout(location = 5) out vec4 outCurrClip;
layout(location = 6) out vec4 outPrevClip;

void main()
{
    vec4 worldPosition = objectData.model * vec4(inPosition, 1.0);
    gl_Position = cameraData.proj * cameraData.view * worldPosition;
    outColor = inColor;
    outTexCoord = inTexCoord;
    outWorldNormal = normalize(mat3(objectData.model) * inNormal);
    outWorldPos = worldPosition.xyz;
    outMaterialIndex = objectData.materialIndex;

    // Interpolate pre-divide clip; reconstruct UV in frag (avoids wrong UV-space lerp on large tris).
    outCurrClip = cameraData.currViewProj * worldPosition;
    outPrevClip = cameraData.prevViewProj * worldPosition;
}

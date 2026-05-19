#version 450

layout(binding = 0) uniform CameraData {
    mat4 model;
    mat4 view;
    mat4 proj;
} cameraData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outWorldNormal;
layout(location = 3) out vec3 outWorldPos;

void main()
{
    vec4 worldPosition = cameraData.model * vec4(inPosition, 1.0);
    gl_Position = cameraData.proj * cameraData.view * worldPosition;
    outColor = inColor;
    outTexCoord = inTexCoord;
    outWorldNormal = normalize(mat3(cameraData.model) * inNormal);
    outWorldPos = worldPosition.xyz;
}

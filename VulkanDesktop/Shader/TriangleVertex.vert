#version 450
// Uniform buffer
layout(binding = 0) uniform CameraData {
    mat4 model;
    mat4 view;
    mat4 proj;
} cameraData;

// Constant buffer

// Input
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

// Output
layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outTexCoord;

void main()
{
    gl_Position = cameraData.proj * cameraData.view * cameraData.model * vec4(inPosition, 1.0);
    outColor = inColor;
    outTexCoord = inTexCoord;
}
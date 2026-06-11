#version 450

layout(set = 0, binding = 0) uniform sampler2D gbufferAlbedo;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(texture(gbufferAlbedo, vUV).rgb, 1.0);
}

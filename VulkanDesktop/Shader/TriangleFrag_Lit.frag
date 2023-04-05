#version 450

// Uniform Buffer
layout(binding = 1) uniform EnvironmentData{
    vec4 fogColor; // w is for exponent
    vec4 fogDistances; // x for min, y for max, zw unused
    vec4 ambientColor;
    vec4 sunlightDirection; // w for sun power
    vec4 sunlightColor;
} envData;

// Input
layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexCoord;

// Output
layout(location = 0) out vec4 outColor;

void main()
{
    //outColor = texture(texSampler, fragTexCoord);
    //outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0);
    outColor = vec4(inColor + envData.ambientColor.xyz, 1.0f);
}
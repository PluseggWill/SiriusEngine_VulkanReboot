// PbrDirect.glsl — direct Cook-Torrance (metallic workflow). Included by lit + deferred + G-buffer; no #version.
// G-buffer contract: RT0.a = metallic, RT1.w = roughness (use Pbr_ClampMetallicRoughness before encode).

const float PBR_PI = 3.14159265358979323846;
const float PBR_MIN_ROUGHNESS = 0.15;  // was 0.04 — too sharp, created glassy specular on rough stone/fabric (perceptual ~0.02)
const float PBR_DIELECTRIC_F0 = 0.04;

vec2 Pbr_ClampMetallicRoughness(float metallic, float roughness)
{
    return vec2(clamp(metallic, 0.0, 1.0), clamp(max(roughness, PBR_MIN_ROUGHNESS), 0.0, 1.0));
}

float Pbr_DistributionGGX(vec3 N, vec3 H, float roughness)
{
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float NdotH = max(dot(N, H), 0.0);
    const float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PBR_PI * denom * denom;
    return a2 / max(denom, 0.0000001);
}

float Pbr_GeometrySchlickGGX(float NdotV, float roughness)
{
    const float r = (roughness + 1.0);
    const float k = (r * r) / 8.0;
    return NdotV / max(NdotV * (1.0 - k) + k, 0.0000001);
}

float Pbr_GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    const float NdotV = max(dot(N, V), 0.0);
    const float NdotL = max(dot(N, L), 0.0);
    return Pbr_GeometrySchlickGGX(NdotV, roughness) * Pbr_GeometrySchlickGGX(NdotL, roughness);
}

vec3 Pbr_FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Full directional term including NdotL; returns vec3(0) when the surface faces away from L.
vec3 Pbr_EvalDirect(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic, float roughness, vec3 lightColor)
{
    const float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) {
        return vec3(0.0);
    }

    const vec3 F0 = mix(vec3(PBR_DIELECTRIC_F0), albedo, metallic);
    const vec3 H = normalize(V + L);

    const float NdotV = max(dot(N, V), 0.0);
    const float HdotV = max(dot(H, V), 0.0);

    const float D = Pbr_DistributionGGX(N, H, roughness);
    const float G = Pbr_GeometrySmith(N, V, L, roughness);
    const vec3 F = Pbr_FresnelSchlick(HdotV, F0);

    const vec3 kS = F;
    const vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    const vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.0001);
    const vec3 diffuse = kD * albedo / PBR_PI;

    return (diffuse + specular) * lightColor * NdotL;
}

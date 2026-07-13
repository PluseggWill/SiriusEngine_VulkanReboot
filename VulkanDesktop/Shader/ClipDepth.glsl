// ClipDepth.glsl — Vulkan ZO clip Z ↔ framebuffer / shadow-compare depth.
// CONTRACT: projections that feed these helpers must emit clip Z in [0,1] (orthoRH_ZO / perspectiveRH_ZO).
// Vulkan viewport: z_fb = z_ndc * (maxDepth - minDepth) + minDepth. With [0,1] range, z_fb == clipZ.
// Do NOT apply OpenGL (z * 0.5 + 0.5) to Z. XY NDC → UV still uses the 0.5 remap.

vec2 Clip_NdcXYToUv(vec2 aNdcXY)
{
    return aNdcXY * 0.5 + 0.5;
}

float Clip_ZToFramebufferDepth(float aClipZ)
{
    return aClipZ;
}

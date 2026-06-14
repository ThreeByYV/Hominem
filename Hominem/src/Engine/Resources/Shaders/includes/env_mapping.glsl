// Cubemap environment mapping: split-sum specular IBL, added on top of the
// existing direct-lit + diffuse-IBL color (diffuse IBL is handled separately
// by ApplyIrradiance in irradiance.glsl).
// Requires scene_ubo.glsl to be included first (uses u_EnvMapIntensity).

uniform samplerCube u_PrefilteredMap; // slot 5, GGX-prefiltered mip chain by roughness
uniform sampler2D   u_BRDFLUT;        // slot 6, RG16F (scale, bias) by (NdotV, roughness)

vec3 ApplyEnvMapping(vec3 color, vec3 N, vec3 V, vec3 albedo, float roughness, float metalness)
{
    if (u_EnvMapIntensity <= 0.0)
        return color;

    vec3 cameraToPixel = -V;
    vec3 reflDir       = normalize(reflect(cameraToPixel, N));

    // Reflection comes from the roughness-selected mip of the prefiltered cube.
    float maxMip      = float(textureQueryLevels(u_PrefilteredMap) - 1);
    vec3  colorReflect = textureLod(u_PrefilteredMap, reflDir, roughness * maxMip).rgb;

    // Split-sum specular: F0 * scale + bias from the BRDF LUT.
    vec3  F0      = mix(vec3(0.04), albedo, metalness);
    float NdotV   = max(dot(N, V), 0.0);
    vec2  envBRDF = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
    vec3  specularIBL = colorReflect * (F0 * envBRDF.x + envBRDF.y);

    return color + specularIBL * u_EnvMapIntensity;
}

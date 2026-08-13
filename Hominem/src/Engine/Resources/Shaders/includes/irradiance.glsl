// Diffuse IBL — samples a pre-convolved irradiance cubemap (see
// irradiance_convolve.glsl) and combines it with the surface albedo.
// Requires pbr.glsl (fresnelSchlickRoughness) and scene_ubo.glsl (u_EnvMapIntensity).

uniform samplerCube u_IrradianceMap; // slot 4

vec3 ApplyIrradiance(vec3 N, vec3 V, vec3 albedo, float roughness, float metalness)
{
    if (u_EnvMapIntensity <= 0.0)
        return vec3(0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metalness);
    vec3 F  = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kD = (1.0 - F) * (1.0 - metalness);

    // Explicit LOD 0. The cubemap is allocated with a full mip chain but the convolve pass
    // only writes level 0, and texture() picks its level from the derivatives of N - which
    // a normal map makes vary sharply, sending the sample into mips that were never
    // written. Nothing to interpolate here anyway: the map is already fully convolved.
    vec3 irradiance = textureLod(u_IrradianceMap, N, 0.0).rgb;
    return kD * irradiance * albedo * u_EnvMapIntensity;
}

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

    vec3 irradiance = texture(u_IrradianceMap, N).rgb;
    return kD * irradiance * albedo * u_EnvMapIntensity;
}

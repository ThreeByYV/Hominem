// Cubemap environment mapping — reflection + refraction with Schlick Fresnel.
// Requires scene_ubo.glsl to be included first (uses u_EnvMapIntensity, u_ETA, u_FresnelPower).

uniform samplerCube u_EnvMap;         // slot 3
uniform samplerCube u_PrefilteredMap; // slot 5, GGX-prefiltered mip chain by roughness

vec3 ApplyEnvMapping(vec3 color, vec3 N, vec3 V, float roughness, float metalness)
{
    if (u_EnvMapIntensity <= 0.0)
        return color;

    vec3 cameraToPixel = -V;
    vec3 reflDir       = normalize(reflect(cameraToPixel, N));
    vec3 refractDir    = normalize(refract(cameraToPixel, N, u_ETA));

    // Reflection comes from the roughness-selected mip of the prefiltered cube;
    // refraction has no roughness term yet so it samples the raw env map.
    float maxMip      = float(textureQueryLevels(u_PrefilteredMap) - 1);
    vec3  colorReflect = textureLod(u_PrefilteredMap, reflDir, roughness * maxMip).rgb;
    vec3  colorRefract = texture(u_EnvMap, refractDir).rgb;

    float F     = ((1.0 - u_ETA) * (1.0 - u_ETA)) / ((1.0 + u_ETA) * (1.0 + u_ETA));
    float ratio = F + (1.0 - F) * pow(clamp(1.0 - dot(V, N), 0.0, 1.0), u_FresnelPower);

    float reflectRatio = mix(ratio, 1.0, metalness);
    vec3  envColor     = mix(colorRefract, colorReflect, reflectRatio);

    float strength = u_EnvMapIntensity * metalness * pow(1.0 - roughness, 4.0);
    return mix(color, envColor, clamp(strength, 0.0, 1.0));
}

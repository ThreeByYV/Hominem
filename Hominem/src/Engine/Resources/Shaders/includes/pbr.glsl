// Cook-Torrance PBR — GGX/Smith/Schlick.

const float PI = 3.14159265359;

float ggxDistribution(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 0.0001);
}

float geomSchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / max(NdotX * (1.0 - k) + k, 0.0001);
}

float geomSmith(float NdotV, float NdotL, float roughness)
{
    return geomSchlickGGX(NdotV, roughness) * geomSchlickGGX(NdotL, roughness);
}

vec3 schlickFresnel(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with a roughness term, used for IBL so rough dielectrics
// don't get an overly strong grazing-angle reflectance boost.
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 evalPBR(vec3 N, vec3 V, vec3 L, vec3 albedo, float roughness, float metalness, vec3 radiance)
{
    vec3  H     = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3  F0 = mix(vec3(0.04), albedo, metalness);
    float D  = ggxDistribution(NdotH, roughness);
    float G  = geomSmith(NdotV, NdotL, roughness);
    vec3  F  = schlickFresnel(VdotH, F0);

    vec3 specBRDF   = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kD         = (1.0 - F) * (1.0 - metalness);
    vec3 diffuseBRDF = kD * albedo / PI;

    return (diffuseBRDF + specBRDF) * radiance * NdotL;
}

// Sphere-area-light version (Karis, "Real Shading in Unreal Engine 4"): nudges L
// toward the closest point on the light's sphere as seen from the reflection
// vector, and widens roughness (with an energy-preserving normalization) so the
// specular highlight has the soft size of the emitter instead of a sharp point.
vec3 evalPBRSphere(vec3 N, vec3 V, vec3 toLight, float lightDist, float sourceRadius,
                   vec3 albedo, float roughness, float metalness, vec3 radiance)
{
    // Degenerates exactly to the punctual case when sourceRadius == 0: centerToRay
    // scales by 0 (L == Ldiffuse) and alphaP == alpha (energyNorm == 1, roughnessP == roughness).
    vec3 Ldiffuse = normalize(toLight);

    vec3 R = reflect(-V, N);
    vec3 centerToRay = dot(toLight, R) * R - toLight;
    vec3 closestPoint = toLight + centerToRay * clamp(sourceRadius / max(length(centerToRay), 0.0001), 0.0, 1.0);
    vec3 L = normalize(closestPoint);

    float alpha  = roughness * roughness;
    float alphaP = clamp(alpha + sourceRadius / (2.0 * max(lightDist, 0.0001)), 0.0, 1.0);

    float roughnessP = sqrt(alphaP);
    float energyNorm = (alpha * alpha) / max(alphaP * alphaP, 0.0001);

    vec3  H     = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3  F0 = mix(vec3(0.04), albedo, metalness);
    float D  = ggxDistribution(NdotH, roughnessP) * energyNorm;
    float G  = geomSmith(NdotV, NdotL, roughnessP);
    vec3  F  = schlickFresnel(VdotH, F0);

    vec3 specBRDF    = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kD          = (1.0 - F) * (1.0 - metalness);
    vec3 diffuseBRDF = kD * albedo / PI;

    float NdotLDiffuse = max(dot(N, Ldiffuse), 0.0);
    return diffuseBRDF * radiance * NdotLDiffuse + specBRDF * radiance * NdotL;
}

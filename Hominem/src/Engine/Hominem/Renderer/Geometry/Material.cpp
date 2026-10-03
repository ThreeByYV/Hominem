#include "hmnpch.h"
#include "Material.h"

#include "Hominem/Assets/MaterialTextures.h"
#include "Hominem/Renderer/RHI/CommandList.h"
#include "Hominem/Renderer/RHI/Shader.h"

namespace Hominem {

void BindMaterial(CommandList& cmd, const Ref<Shader>& shader, const Material& material)
{
    auto bind = [&](uint32_t slot, const Ref<Texture2D>& tex, const Ref<Texture2D>& fallback)
    {
        cmd.BindTexture(slot, (tex ? tex : fallback)->GetRendererID());
    };
    // White: a missing map leaves just the factor.
    bind(MaterialSlot::BaseColor,      material.BaseColor,      WhiteTexture());
    bind(MaterialSlot::MetalRoughness, material.MetalRoughness, WhiteTexture());
    bind(MaterialSlot::Normal,         material.Normal,         FlatNormalMap());
    bind(MaterialSlot::Emissive,       material.Emissive,       WhiteTexture());

    cmd.SetInt(shader, "u_Albedo",         (int)MaterialSlot::BaseColor);
    cmd.SetInt(shader, "u_MetalRoughness", (int)MaterialSlot::MetalRoughness);
    cmd.SetInt(shader, "u_NormalMap",      (int)MaterialSlot::Normal);
    cmd.SetInt(shader, "u_Emissive",       (int)MaterialSlot::Emissive);

    cmd.SetFloat4(shader, "u_BaseColorFactor", material.BaseColorFactor);
    cmd.SetFloat (shader, "u_Metalness",       material.Metallic);
    cmd.SetFloat (shader, "u_Roughness",       material.Roughness);
    cmd.SetFloat3(shader, "u_EmissiveFactor",  material.EmissiveFactor);
}

}

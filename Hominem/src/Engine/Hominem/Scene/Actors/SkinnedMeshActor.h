#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Renderer/Shader.h"
#include "Hominem/Renderer/RenderFrame.h"

#include <vector>

namespace Hominem {

    /// Base actor for rendering a skinned mesh with animation blending.
    /// AnimTime is advanced in OnUpdate each frame. Subclasses set Mesh and
    /// BlendFactor in their OnUpdate, then call SkinnedMeshActor::OnUpdate(ts).
    class SkinnedMeshActor : public Actor
    {
    public:
        Ref<SkinnedMesh> Mesh;

        float    AnimTime          = 0.f;
        uint32_t BlendFromAnim     = 1;
        uint32_t BlendToAnim       = 2;
        float    BlendFactor       = 0.f;   ///< 0 = full BlendFromAnim, 1 = full BlendToAnim
        bool     DisableRootMotion = true;
        Ref<Shader> ShaderOverride;

        void OnUpdate(Timestep ts) override
        {
            AnimTime += static_cast<float>(ts);
        }

        void OnBuildRenderFrame(RenderFrame& frame) override
        {
            if (!Mesh) return;

            int boneCount = Mesh->GetBoneCount();
            if (boneCount <= 0)
            {
                frame.meshes.push_back({ Mesh, GetTransform(), {}, ShaderOverride });
                return;
            }

            m_BoneTransforms.resize(boneCount);

            if (Mesh->GetAnimationCount() >= 2)
                Mesh->GetBoneTransformsBlended(AnimTime, m_BoneTransforms,
                    BlendFromAnim, BlendToAnim, BlendFactor, DisableRootMotion);
            else
                Mesh->GetBoneTransformsBlended(AnimTime, m_BoneTransforms,
                    1, 1, 1.f, DisableRootMotion);

            auto bones = frame.AllocBones(boneCount);
            std::copy(m_BoneTransforms.begin(), m_BoneTransforms.end(), bones.begin());
            frame.meshes.push_back({ Mesh, GetTransform(), bones, ShaderOverride });
        }

    private:
        std::vector<glm::mat4> m_BoneTransforms;
    };

}

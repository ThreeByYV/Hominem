#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Renderer/Geometry/SkinnedMesh.h"
#include "Hominem/Renderer/RHI/Shader.h"
#include "Hominem/Renderer/Frame/RenderFrame.h"

#include <algorithm>
#include <vector>

namespace Hominem {

    /// Base actor for rendering a skinned mesh with animation blending.
    /// AnimTime is advanced in OnUpdate each frame. Subclasses set Mesh and
    /// BlendFactor in their OnUpdate, then call SkinnedMeshActor::OnUpdate(ts).
    class SkinnedMeshActor : public Actor
    {
    public:
        Ref<SkinnedMesh> Mesh;

        float    AnimTime          = 0.f;   ///< BlendToAnim's playback time, seconds
        float    BlendFromTime     = 0.f;   ///< BlendFromAnim's, so an outgoing clip keeps moving
        uint32_t BlendFromAnim     = 1;
        uint32_t BlendToAnim       = 2;
        float    BlendFactor       = 0.f;   ///< 0 = full BlendFromAnim, 1 = full BlendToAnim
        bool     DisableRootMotion = true;
        Ref<Shader> ShaderOverride;

        /// Plays a clip by name from the start, crossfading from the current pose over
        /// `fadeSeconds` (0 cuts). False if the mesh has no such clip.
        bool PlayAnimation(std::string_view name, float fadeSeconds = 0.f)
        {
            const auto slot = Mesh ? Mesh->FindAnimation(name) : std::nullopt;
            if (!slot) return false;

            if (fadeSeconds > 0.f)
            {
                // Freeze the blend where it is: the dominant clip becomes the outgoing one.
                const bool toDominant = BlendFactor >= 0.5f;
                BlendFromAnim = toDominant ? BlendToAnim : BlendFromAnim;
                BlendFromTime = toDominant ? AnimTime    : BlendFromTime;
                BlendFactor   = 0.f;
            }
            else
            {
                BlendFromAnim = *slot;
                BlendFromTime = 0.f;
                BlendFactor   = 1.f;
            }
            BlendToAnim   = *slot;
            AnimTime      = 0.f;
            m_FadeSeconds = fadeSeconds;
            return true;
        }

        /// True once the clip started by PlayAnimation has played through (the clock wraps after).
        bool IsClipFinished() const
        {
            return Mesh && AnimTime >= Mesh->GetAnimationDuration(BlendToAnim);
        }

        /// Bone matrices for the current clips, times and blend.
        void SamplePose(std::vector<glm::mat4>& out) const
        {
            // Slot 0 is the main animation; a single-animation mesh only has that one.
            if (Mesh->GetAnimationCount() < 2)
            {
                Mesh->GetBoneTransformsBlended(AnimTime, out, 0, 0, 1.f, DisableRootMotion);
                return;
            }
            const std::vector<AnimBlendSample> samples
            {
                { BlendFromAnim, BlendFromTime, 1.f - BlendFactor },
                { BlendToAnim,   AnimTime,      BlendFactor       },
            };
            Mesh->GetBoneTransformsBlendedN(samples, out, DisableRootMotion);
        }

        void OnUpdate(Timestep ts) override
        {
            const float dt = static_cast<float>(ts);
            AnimTime      += dt;
            BlendFromTime += dt;
            if (m_FadeSeconds > 0.f)
            {
                BlendFactor = std::min(1.f, BlendFactor + dt / m_FadeSeconds);
                if (BlendFactor >= 1.f) m_FadeSeconds = 0.f;
            }
        }

        void OnBuildRenderFrame(RenderFrame& frame) override
        {
            if (!Mesh) return;

            int boneCount = Mesh->GetBoneCount();
            if (boneCount <= 0)
            {
                frame.meshes.push_back({ Mesh, GetWorldTransform(), {}, ShaderOverride });
                return;
            }

            m_BoneTransforms.resize(boneCount);
            SamplePose(m_BoneTransforms);

            auto bones = frame.AllocBones(boneCount);
            std::copy(m_BoneTransforms.begin(), m_BoneTransforms.end(), bones.begin());
            frame.meshes.push_back({ Mesh, GetWorldTransform(), bones, ShaderOverride });
        }

    private:
        std::vector<glm::mat4> m_BoneTransforms;
        float                  m_FadeSeconds = 0.f; ///< > 0 while a crossfade is running
    };

}

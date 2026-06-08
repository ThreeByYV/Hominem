#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Renderer/Renderer3D.h"

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


class SilhouetteCharacterActor : public Hominem::Actor
{
public:
	explicit SilhouetteCharacterActor(Hominem::Ref<Hominem::SkinnedMesh> mesh)
		: m_Mesh(std::move(mesh)) {}

	void OnCreate() override
	{
		m_Shader = Hominem::Renderer3D::GetShaderLibrary()->Get("silhouette");
	}

	void OnUpdate(Hominem::Timestep ts) override
	{
		m_AnimTime += (float)ts * AnimSpeed;
	}

	void OnBuildRenderFrame(Hominem::RenderFrame& frame) override
	{
		if (!m_Mesh || m_Mesh->GetBoneCount() == 0) return;

		// Layer manual arm poses on the idle clip (fire.png hand-hold). Set transiently on
		// the shared skeleton per-sample so mesh-sharing figures each get their own pose.
		auto& skel = m_Mesh->GetSkeleton();
		skel.ClearPoseOverrides();
		ApplyArm(skel, "mixamorig:RightArm", RightArmDeg);
		ApplyArm(skel, "mixamorig:LeftArm",  LeftArmDeg);
		ApplyArm(skel, "mixamorig:RightForeArm", RightForeArmDeg);
		ApplyArm(skel, "mixamorig:LeftForeArm",  LeftForeArmDeg);

		m_Mesh->GetBoneTransforms(m_AnimTime, m_BoneTransforms, /*disableRootMotion=*/true);

		auto bones = frame.AllocBones(m_BoneTransforms.size());
		std::copy(m_BoneTransforms.begin(), m_BoneTransforms.end(), bones.begin());

		// Silhouette = flat-black override shader (foreground figures); otherwise pass no
		// override so the renderer picks the normal lit skinned variant (the runners).
		Hominem::Ref<Hominem::Shader> shader = Silhouette ? m_Shader : nullptr;
		frame.meshes.push_back({ m_Mesh, GetTransform(), bones, shader });
	}

	float AnimSpeed  = 1.0f;
	bool  Silhouette = true; ///< false → render with the normal lit shader instead of flat black.

	// Manual arm posing (local-space euler degrees) layered on the idle clip - used to
	// reach the foreground pair's arms together. Zero = leave the animated pose untouched.
	glm::vec3 RightArmDeg     { 0.f };
	glm::vec3 LeftArmDeg      { 0.f };
	glm::vec3 RightForeArmDeg { 0.f };
	glm::vec3 LeftForeArmDeg  { 0.f };

private:
	// Inject an extra local rotation at a joint (no-op when the euler is all zero).
	static void ApplyArm(Hominem::Skeleton& skel, const char* bone, const glm::vec3& deg)
	{
		if (deg.x == 0.f && deg.y == 0.f && deg.z == 0.f) return;
		glm::mat4 r(1.f);
		r = glm::rotate(r, glm::radians(deg.y), { 0.f, 1.f, 0.f });
		r = glm::rotate(r, glm::radians(deg.x), { 1.f, 0.f, 0.f });
		r = glm::rotate(r, glm::radians(deg.z), { 0.f, 0.f, 1.f });
		skel.SetBonePoseOverride(bone, r);
	}

	Hominem::Ref<Hominem::SkinnedMesh>   m_Mesh;
	Hominem::Ref<Hominem::Shader>        m_Shader;
	float                                m_AnimTime = 0.f;
	std::vector<glm::mat4>               m_BoneTransforms;
};

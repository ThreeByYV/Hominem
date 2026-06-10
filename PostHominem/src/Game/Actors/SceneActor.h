#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Renderer/StaticMesh.h"
#include "Hominem/Renderer/Renderer3D.h"

#include <string>

class SceneActor : public Hominem::Actor
{
public:
	explicit SceneActor(std::string meshPath)
		: m_MeshPath(std::move(meshPath)) {}

	/// Preloaded mesh — skips the synchronous LoadFromFile in OnCreate. Used when
	/// the mesh was already loaded on a background thread (e.g. CutscenePreload),
	/// so spawning the actor doesn't stall the main thread on a big GLB import.
	explicit SceneActor(Hominem::Ref<Hominem::StaticMesh> mesh)
		: m_Mesh(std::move(mesh)) {}

	void OnCreate() override
	{
		if (m_Mesh) return;
		m_Mesh = Hominem::StaticMesh::Create();
		if (auto res = m_Mesh->LoadFromFile(m_MeshPath); !res)
			HMN_CORE_ERROR("{}", res.error());
	}

	void OnBuildRenderFrame(Hominem::RenderFrame& frame) override
	{
		if (!m_Mesh || !m_Mesh->IsLoaded()) return;
		if (!frame.frustum3D.TestAABBTransformed(
				m_Mesh->GetAABBMin(), m_Mesh->GetAABBMax(), GetTransform()))
			return;
		uint64_t key = reinterpret_cast<uint64_t>(m_Mesh.get());
		frame.staticMeshes.push_back({ m_Mesh, GetTransform(), key });
	}

	Hominem::StaticMesh* GetMesh() const { return m_Mesh.get(); }

private:
	std::string                       m_MeshPath;
	Hominem::Ref<Hominem::StaticMesh> m_Mesh;
};

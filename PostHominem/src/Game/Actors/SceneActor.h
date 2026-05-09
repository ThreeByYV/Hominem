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

	void OnCreate() override
	{
		m_Mesh = Hominem::CreateRef<Hominem::StaticMesh>();
		if (!m_Mesh->LoadFromFile(m_MeshPath))
			HMN_CORE_ERROR("SceneActor: failed to load '{}'", m_MeshPath);
	}

	void OnBuildRenderFrame(Hominem::RenderFrame& frame) override
	{
		if (!m_Mesh || !m_Mesh->IsLoaded()) return;
		frame.staticMeshes.push_back({ m_Mesh, GetTransform() });
	}

	Hominem::StaticMesh* GetMesh() const { return m_Mesh.get(); }

private:
	std::string                       m_MeshPath;
	Hominem::Ref<Hominem::StaticMesh> m_Mesh;
};

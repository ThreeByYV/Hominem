#pragma once

#include "Hominem/Renderer/StaticMesh.h"
#include "Hominem/Assets/MeshData.h"

#include <vector>

namespace Hominem {

class OpenGLStaticMesh : public StaticMesh
{
public:
	OpenGLStaticMesh()  = default;
	~OpenGLStaticMesh() override;

	[[nodiscard]] std::expected<void, std::string> LoadFromFile(const std::string& path) override;

	std::pair<uint32_t, uint64_t> Draw(const Ref<Shader>& shader,
	                                    const glm::mat4&   actorTransform,
	                                    const Frustum*     frustum = nullptr) override;

	bool IsLoaded()        const override { return m_VAO != 0 || !m_PendingVerts.empty(); }

	glm::vec3 GetAABBMin() const override { return m_AABBMin; }
	glm::vec3 GetAABBMax() const override { return m_AABBMax; }

	bool HasNormalMap()      const override;
	bool HasMetalRoughness() const override;

	size_t   GetDrawGroupCount() const override { return m_DrawGroups.size(); }
	uint64_t GetTriangleCount()  const override
	{
		uint64_t t = 0;
		for (const auto& g : m_DrawGroups) t += g.IndexCount / 3;
		return t;
	}

	GroupBounds GetDrawGroupBounds(size_t i)    const override { return { m_DrawGroups[i].AABBMin, m_DrawGroups[i].AABBMax }; }
	glm::mat4   GetDrawGroupTransform(size_t i) const override { return m_DrawGroups[i].NodeTransform; }

private:
	void Upload(const std::vector<StaticVertex>& verts, const std::vector<uint32_t>& indices);

	uint32_t                   m_VAO               = 0;
	uint32_t                   m_VBO               = 0;
	uint32_t                   m_IBO               = 0;
	uint32_t                   m_ModelMatrixSSBO   = 0;
	uint32_t                   m_DrawCommandBuffer = 0;
	std::vector<MeshDrawGroup> m_DrawGroups;
	glm::vec3                  m_AABBMin { 0.f };
	glm::vec3                  m_AABBMax { 0.f };

	std::vector<StaticVertex> m_PendingVerts;
	std::vector<uint32_t>     m_PendingIndices;
};

}

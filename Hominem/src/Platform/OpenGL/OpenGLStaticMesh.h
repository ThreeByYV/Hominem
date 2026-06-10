#pragma once

#include "Hominem/Renderer/StaticMesh.h"

#include <cfloat>
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
	struct StaticVertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoord;
		glm::vec4 Tangent;   // xyz = tangent, w = handedness
	};

	struct DrawGroup
	{
		Ref<Texture2D> Albedo;
		Ref<Texture2D> MetalRoughness;
		Ref<Texture2D> NormalMap;
		bool           HasRealMetalRoughness = false;
		bool           HasRealNormalMap      = false;
		uint32_t       IndexByteOffset = 0;
		uint32_t       IndexCount      = 0;
		int32_t        BaseVertex      = 0;
		glm::vec3      AABBMin {  FLT_MAX };
		glm::vec3      AABBMax { -FLT_MAX };
		glm::mat4      NodeTransform { 1.f };
	};

	bool LoadBinary(const std::string& binPath);
	[[nodiscard]] std::expected<void, std::string> LoadAssimp(const std::string& path);

	static void OptimizeGeometry(std::vector<StaticVertex>& verts,
	                              std::vector<uint32_t>&     indices,
	                              std::vector<DrawGroup>&    groups);
	void WriteBinary(const std::string& binPath,
	                 const std::vector<StaticVertex>& verts,
	                 const std::vector<uint32_t>&     indices,
	                 const std::vector<DrawGroup>&    groups,
	                 const std::vector<std::string>&  groupAlbedoPaths,
	                 const std::vector<std::string>&  groupMRPaths,
	                 const std::vector<std::string>&  groupNormalPaths);
	void Upload(const std::vector<StaticVertex>& verts, const std::vector<uint32_t>& indices);
	void ComputeWorldAABB();

	uint32_t               m_VAO               = 0;
	uint32_t               m_VBO               = 0;
	uint32_t               m_IBO               = 0;
	uint32_t               m_ModelMatrixSSBO   = 0;
	uint32_t               m_DrawCommandBuffer = 0;
	std::vector<DrawGroup> m_DrawGroups;
	glm::vec3              m_AABBMin { 0.f };
	glm::vec3              m_AABBMax { 0.f };

	std::vector<StaticVertex> m_PendingVerts;
	std::vector<uint32_t>     m_PendingIndices;
};

}

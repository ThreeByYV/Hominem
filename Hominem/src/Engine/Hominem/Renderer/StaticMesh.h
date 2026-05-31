#pragma once

#include "Shader.h"
#include "ShaderPermutation.h"
#include "Texture.h"
#include "Material.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <utility>

namespace Hominem {

struct Frustum;

class StaticMesh : public RefCounted
{
public:
	StaticMesh()  = default;
	~StaticMesh();

	bool LoadFromFile(const std::string& path);

	// Returns {draw calls issued, triangles rendered}.
	std::pair<uint32_t, uint64_t> Draw(const Ref<Shader>& shader,
	                                    const glm::mat4&   actorTransform,
	                                    const Frustum*     frustum = nullptr);

	bool IsLoaded() const { return m_VAO != 0 || !m_PendingVerts.empty(); }

	glm::vec3 GetAABBMin() const { return m_AABBMin; }
	glm::vec3 GetAABBMax() const { return m_AABBMax; }

	const Material& GetMaterial() const              { return m_Material; }
	void            SetMaterial(const Material& mat) { m_Material = mat; }

	uint32_t GetPermutationFlags() const;

	size_t   GetDrawGroupCount() const { return m_DrawGroups.size(); }
	uint64_t GetTriangleCount()  const
	{
		uint64_t t = 0;
		for (const auto& g : m_DrawGroups) t += g.IndexCount / 3;
		return t;
	}

	struct GroupBounds { glm::vec3 Min, Max; };

	GroupBounds GetDrawGroupBounds(size_t i) const
	{
		return { m_DrawGroups[i].AABBMin, m_DrawGroups[i].AABBMax };
	}

	glm::mat4 GetDrawGroupTransform(size_t i) const
	{
		return m_DrawGroups[i].NodeTransform;
	}

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
		glm::vec3      AABBMin {  FLT_MAX };  // local (node) space
		glm::vec3      AABBMax { -FLT_MAX };
		glm::mat4      NodeTransform { 1.f }; // accumulated world transform from GLTF node tree
	};

	bool LoadBinary(const std::string& binPath);
	bool LoadAssimp(const std::string& path);
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

	Material               m_Material;
	uint32_t               m_VAO = 0;
	uint32_t               m_VBO = 0;
	uint32_t               m_IBO = 0;
	std::vector<DrawGroup> m_DrawGroups;
	glm::vec3              m_AABBMin { 0.f };
	glm::vec3              m_AABBMax { 0.f };

	std::vector<StaticVertex> m_PendingVerts;
	std::vector<uint32_t>     m_PendingIndices;
};

}

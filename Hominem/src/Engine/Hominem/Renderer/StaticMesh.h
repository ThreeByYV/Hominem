#pragma once

#include "Shader.h"
#include "ShaderPermutation.h"
#include "Texture.h"
#include "Material.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Hominem {

class StaticMesh : public RefCounted
{
public:
	StaticMesh()  = default;
	~StaticMesh();

	// Checks for a .bin sidecar cache first; falls back to assimp and writes the cache.
	bool LoadFromFile(const std::string& path);
	void Draw(const Ref<Shader>& shader, const glm::mat4& transform);

	// True once geometry is on GPU — false until first Draw() on the render thread.
	bool IsLoaded() const { return m_VAO != 0 || !m_PendingVerts.empty(); }

	glm::vec3 GetAABBMin() const { return m_AABBMin; }
	glm::vec3 GetAABBMax() const { return m_AABBMax; }

	const Material& GetMaterial() const              { return m_Material; }
	void            SetMaterial(const Material& mat) { m_Material = mat; }

	// Returns ShaderPerm flags describing which PBR features this mesh actually has textures for.
	uint32_t GetPermutationFlags() const;

	size_t   GetDrawGroupCount() const { return m_DrawGroups.size(); }
	uint64_t GetTriangleCount()  const
	{
		uint64_t t = 0;
		for (const auto& g : m_DrawGroups) t += g.IndexCount / 3;
		return t;
	}

private:
	struct StaticVertex
	{
		glm::vec3 Position;  // offset  0
		glm::vec3 Normal;    // offset 12
		glm::vec2 TexCoord;  // offset 24
		glm::vec4 Tangent;   // offset 32 — xyz = tangent, w = handedness (-1 or +1)
	};                       // total  48 bytes

	struct DrawGroup
	{
		Ref<Texture2D> Albedo;
		Ref<Texture2D> MetalRoughness;      // G = roughness, B = metalness
		Ref<Texture2D> NormalMap;
		bool           HasRealMetalRoughness = false; // false = default 0.5/0 fallback
		bool           HasRealNormalMap      = false; // false = flat (0,0,1) fallback
		uint32_t       IndexByteOffset;
		uint32_t       IndexCount;
		int32_t        BaseVertex;
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

	Material               m_Material;
	uint32_t               m_VAO = 0;
	uint32_t               m_VBO = 0;
	uint32_t               m_IBO = 0;
	std::vector<DrawGroup> m_DrawGroups;
	glm::vec3              m_AABBMin{ 0.f };
	glm::vec3              m_AABBMax{ 0.f };

	// CPU-side geometry held until first Draw() on the render thread.
	std::vector<StaticVertex> m_PendingVerts;
	std::vector<uint32_t>     m_PendingIndices;
};

}

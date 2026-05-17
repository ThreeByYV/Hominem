#pragma once

#include "Shader.h"
#include "Texture.h"
#include "Material.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Hominem {

class StaticMesh
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

	const Material& GetMaterial() const        { return m_Material; }
	void            SetMaterial(const Material& mat) { m_Material = mat; }

private:
	struct StaticVertex
	{
		glm::vec3 Position;  // offset  0
		glm::vec3 Normal;    // offset 12
		glm::vec2 TexCoord;  // offset 24
	};                       // total  32 bytes

	struct DrawGroup
	{
		Ref<Texture2D> Texture;
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
	                 const std::vector<std::string>&  groupTexPaths);
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

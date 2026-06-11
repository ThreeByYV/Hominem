#pragma once

#include "Shader.h"
#include "Texture.h"
#include "Material.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <utility>
#include <expected>

namespace Hominem {

struct Frustum;

class StaticMesh : public RefCounted
{
public:
	virtual ~StaticMesh() = default;

	[[nodiscard]] virtual std::expected<void, std::string> LoadFromFile(const std::string& path) = 0;

	// Returns {draw calls issued, triangles rendered}.
	virtual std::pair<uint32_t, uint64_t> Draw(const Ref<Shader>& shader,
	                                            const glm::mat4&   actorTransform,
	                                            const Frustum*     frustum = nullptr) = 0;

	virtual bool IsLoaded()        const = 0;

	virtual glm::vec3 GetAABBMin() const = 0;
	virtual glm::vec3 GetAABBMax() const = 0;

	virtual bool HasNormalMap()      const = 0;
	virtual bool HasMetalRoughness() const = 0;

	virtual size_t   GetDrawGroupCount() const = 0;
	virtual uint64_t GetTriangleCount()  const = 0;

	struct GroupBounds { glm::vec3 Min, Max; };

	virtual GroupBounds GetDrawGroupBounds(size_t i)    const = 0;
	virtual glm::mat4   GetDrawGroupTransform(size_t i) const = 0;

	const Material& GetMaterial() const              { return m_Material; }
	void            SetMaterial(const Material& mat) { m_Material = mat; }

	static Ref<StaticMesh> Create();

protected:
	Material m_Material;
};

}

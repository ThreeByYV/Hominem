#pragma once

#include <glm/glm.hpp>
#include <array>
#include <cfloat>

namespace Hominem {

struct Frustum
{
	// 6 planes stored as vec4(normal.xyz, d).
	// A point p is inside plane i when: dot(planes[i].xyz, p) + planes[i].w >= 0
	std::array<glm::vec4, 6> planes {};

	// Extract frustum planes from a combined view-projection matrix.
	// Gribb/Hartmann method — works for OpenGL NDC (z in [-1, 1]).
	static Frustum FromViewProjection(const glm::mat4& vp)
	{
		// GLM is column-major: vp[col][row], so row i = (vp[0][i], vp[1][i], vp[2][i], vp[3][i])
		auto row = [&](int i) {
			return glm::vec4(vp[0][i], vp[1][i], vp[2][i], vp[3][i]);
		};

		const glm::vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);

		Frustum f;
		f.planes[0] = r3 + r0;  // left
		f.planes[1] = r3 - r0;  // right
		f.planes[2] = r3 + r1;  // bottom
		f.planes[3] = r3 - r1;  // top
		f.planes[4] = r3 + r2;  // near
		f.planes[5] = r3 - r2;  // far

		for (auto& p : f.planes)
		{
			float len = glm::length(glm::vec3(p));
			if (len > 0.f) p /= len;
		}
		return f;
	}

	// Returns true if the world-space AABB is at least partially inside the frustum.
	// Uses the positive-vertex method — O(6) regardless of mesh complexity.
	bool TestAABB(const glm::vec3& worldMin, const glm::vec3& worldMax) const
	{
		for (const auto& plane : planes)
		{
			// Positive vertex: the corner of the AABB most aligned with the plane normal.
			// If even this corner is outside (behind the plane), the whole box is outside.
			const glm::vec3 pv(
				plane.x >= 0.f ? worldMax.x : worldMin.x,
				plane.y >= 0.f ? worldMax.y : worldMin.y,
				plane.z >= 0.f ? worldMax.z : worldMin.z
			);
			if (glm::dot(glm::vec3(plane), pv) + plane.w < 0.f)
				return false;
		}
		return true;
	}

	// Convenience overload: transforms a model-space AABB by 'transform' into world
	// space (conservative — takes the world-space AABB of the 8 transformed corners)
	// then tests it. Use this when the mesh AABB is in local space.
	bool TestAABBTransformed(const glm::vec3& localMin, const glm::vec3& localMax,
	                          const glm::mat4& transform) const
	{
		glm::vec3 worldMin( FLT_MAX);
		glm::vec3 worldMax(-FLT_MAX);

		for (int i = 0; i < 8; i++)
		{
			const glm::vec3 corner(
				(i & 1) ? localMax.x : localMin.x,
				(i & 2) ? localMax.y : localMin.y,
				(i & 4) ? localMax.z : localMin.z
			);
			const glm::vec3 wc = glm::vec3(transform * glm::vec4(corner, 1.f));
			worldMin = glm::min(worldMin, wc);
			worldMax = glm::max(worldMax, wc);
		}
		return TestAABB(worldMin, worldMax);
	}
};

}

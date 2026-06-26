#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Hominem {

/// For building quad/mesh transforms inline without chaining glm calls.
struct Transform
{
    glm::vec3 position { 0.f };
    glm::vec3 rotation { 0.f }; // radians XYZ euler
    glm::vec3 scale    { 1.f };

    [[nodiscard]] glm::mat4 ToMatrix() const
    {
        return glm::translate(glm::mat4(1.f), position)
             * glm::toMat4(glm::quat(rotation))
             * glm::scale(glm::mat4(1.f), scale);
    }

    static Transform Pos(glm::vec3 p)
        { return { p, {}, { 1.f, 1.f, 1.f } }; }

    static Transform PosScale(glm::vec3 p, glm::vec3 s)
        { return { p, {}, s }; }

    static Transform PosRotScale(glm::vec3 p, glm::vec3 r, glm::vec3 s)
        { return { p, r, s }; }

    /// For full-screen or 2D overlay quads where only XY scale matters.
    static Transform Scale2D(glm::vec2 s)
        { return { {}, {}, { s.x, s.y, 1.f } }; }

    static Transform Scale2D(float x, float y)
        { return { {}, {}, { x, y, 1.f } }; }
};

}

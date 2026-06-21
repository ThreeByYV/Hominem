#pragma once

#include "Hominem/Core/Core.h"
#include "Rigidbody.h"
#include "Collider.h"

#include <glm/glm.hpp>

namespace Hominem {
    class PhysicsWorld;

    namespace Physics {

        /// Creates a 2.5D dynamic body at pos with all rotation + Z translation locked.
        Ref<Rigidbody> CreateDynamicBody2D(PhysicsWorld* world, glm::vec3 pos, float mass = 1.f);

        /// Creates a static body at pos with gravity disabled.
        Ref<Rigidbody> CreateStaticBody(PhysicsWorld* world, glm::vec3 pos);

        /// Creates a material, builds a box collider, attaches it to body, and returns the collider.
        Ref<BoxCollider> AttachBoxCollider(PhysicsWorld* world, const Ref<Rigidbody>& body,
                                           glm::vec3 halfExtents, glm::vec3 offset = {},
                                           float staticFriction = 0.5f, float dynamicFriction = 0.5f);
    }
}

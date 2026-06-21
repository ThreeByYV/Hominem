#include "hmnpch.h"
#include "PhysicsHelpers.h"
#include "PhysicsWorld.h"
#include "PhysicsTypes.h"

namespace Hominem::Physics {

    Ref<Rigidbody> CreateDynamicBody2D(PhysicsWorld* world, glm::vec3 pos, float mass)
    {
        if (!world) return nullptr;
        RigidbodySpec spec;
        spec.Type             = RigidbodyType::Dynamic;
        spec.Position         = pos;
        spec.Mass             = mass;
        spec.LockRotationX    = true;
        spec.LockRotationY    = true;
        spec.LockRotationZ    = true;
        spec.LockTranslationZ = true;
        auto body = world->CreateRigidbody(spec);
        body->SetGravityEnabled(true);
        return body;
    }

    Ref<Rigidbody> CreateStaticBody(PhysicsWorld* world, glm::vec3 pos)
    {
        if (!world) return nullptr;
        RigidbodySpec spec;
        spec.Type     = RigidbodyType::Static;
        spec.Position = pos;
        auto body = world->CreateRigidbody(spec);
        body->SetGravityEnabled(false);
        return body;
    }

    Ref<BoxCollider> AttachBoxCollider(PhysicsWorld* world, const Ref<Rigidbody>& body,
                                       glm::vec3 halfExtents, glm::vec3 offset,
                                       float staticFriction, float dynamicFriction)
    {
        if (!world || !body) return nullptr;
        auto mat = world->CreateMaterial(staticFriction, dynamicFriction, 0.f);
        BoxColliderSpec col;
        col.HalfExtents = halfExtents;
        col.Offset      = offset;
        auto collider = world->CreateBoxCollider(col, mat);
        body->AttachCollider(collider);
        return collider;
    }

}

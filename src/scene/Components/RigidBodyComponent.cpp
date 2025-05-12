#include "RigidBodyComponent.h"
#include "physics/PhysicsSystem.h"      // For adding/removing body from the world
#include "physics/EngineMotionState.h"  // Custom motion state for transform sync
#include "scene/GameObject.h"           // To get TransformComponent and name
#include "scene/Components/TransformComponent.h" // For transform access
#include "core/Log.h"                   // For logging

// --- Include Actual Bullet Headers for Shape Creation ---
#include <btBulletDynamicsCommon.h> // Includes most common Bullet headers
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionShapes/btCylinderShape.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h> // For Convex Hull
// #include <BulletCollision/CollisionShapes/btStaticPlaneShape.h> // If adding PLANE shape

namespace VulkEng {

    RigidBodyComponent::RigidBodyComponent(const RigidBodySettings& settings)
        : m_Settings(settings),
          m_CachedTransformComponent(nullptr),
          m_Shape(nullptr),
          m_MotionState(nullptr),
          m_RigidBody(nullptr),
          m_IsInitialized(false)
    {
        // VKENG_TRACE("RigidBodyComponent created with mass: {}", m_Settings.mass);
    }

    RigidBodyComponent::~RigidBodyComponent() {
        // VKENG_TRACE("RigidBodyComponent for GameObject '{}' destructing...",
        //             m_GameObject ? m_GameObject->GetName() : "UNATTACHED/DETACHED");

        // CleanupPhysics should ideally be called explicitly before component destruction
        // (e.g., in GameObject::RemoveComponent or Scene cleanup).
        // If not, the unique_ptrs for Bullet objects will handle their memory,
        // but the rigid body might still be in the PhysicsSystem's world if not removed.
        if (m_IsInitialized) {
            VKENG_WARN("RigidBodyComponent for '{}' destroyed without explicit CleanupPhysics call. "
                       "Body might still be in physics world if PhysicsSystem still exists.",
                       m_GameObject ? m_GameObject->GetName() : "UNKNOWN");
            // If m_RigidBody and its PhysicsSystem are still valid, attempt removal. This is risky.
            // Better to enforce explicit cleanup.
        }
        // unique_ptr members (m_Shape, m_MotionState, m_RigidBody) automatically delete Bullet objects.
    }

    void RigidBodyComponent::CreateShape() {
        if (m_Shape) { // Clear existing shape if any (e.g., if re-initializing)
            m_Shape.reset();
        }

        const std::string gameObjectName = m_GameObject ? m_GameObject->GetName() : "ShapeCreation";

        switch (m_Settings.shapeType) {
            case CollisionShapeType::BOX:
                // dimensions are half-extents
                m_Shape = std::make_unique<btBoxShape>(
                    btVector3(m_Settings.dimensions.x, m_Settings.dimensions.y, m_Settings.dimensions.z)
                );
                VKENG_TRACE("RigidBody '{}': Created btBoxShape.", gameObjectName);
                break;

            case CollisionShapeType::SPHERE:
                // dimensions.x is radius
                m_Shape = std::make_unique<btSphereShape>(btScalar(m_Settings.dimensions.x));
                VKENG_TRACE("RigidBody '{}': Created btSphereShape.", gameObjectName);
                break;

            case CollisionShapeType::CAPSULE:
                // dimensions.x is radius, dimensions.y is height (of the cylindrical part)
                // Bullet has different capsule shapes (X, Y, Z aligned)
                // btCapsuleShape is Y-aligned by default.
                m_Shape = std::make_unique<btCapsuleShape>(
                    btScalar(m_Settings.dimensions.x), // radius
                    btScalar(m_Settings.dimensions.y)  // height
                );
                VKENG_TRACE("RigidBody '{}': Created btCapsuleShape (Y-aligned).", gameObjectName);
                break;

            case CollisionShapeType::CYLINDER:
                // dimensions are half-extents (radius.x, height/2, radius.z if non-uniform)
                // btCylinderShape is Y-aligned by default.
                m_Shape = std::make_unique<btCylinderShape>(
                    btVector3(m_Settings.dimensions.x, m_Settings.dimensions.y, m_Settings.dimensions.z)
                );
                VKENG_TRACE("RigidBody '{}': Created btCylinderShape (Y-aligned).", gameObjectName);
                break;

            case CollisionShapeType::CONVEX_HULL:
                VKENG_INFO("RigidBody '{}': Creating btConvexHullShape...", gameObjectName);
                if (m_Settings.physicsVertices.empty()) {
                    VKENG_ERROR("ConvexHull shape requested for '{}' but no physics vertices provided! Creating default box.", gameObjectName);
                    m_Shape = std::make_unique<btBoxShape>(btVector3(0.5f, 0.5f, 0.5f));
                } else {
                    auto hullShape = std::make_unique<btConvexHullShape>();
                    for (const auto& vert : m_Settings.physicsVertices) {
                        hullShape->addPoint(btVector3(vert.x, vert.y, vert.z), false); // 'false' to not recalc AABB yet
                    }
                    hullShape->recalcLocalAabb(); // Recalc AABB once all points are added
                    // Optional: hullShape->optimizeConvexHull();
                    m_Shape = std::move(hullShape);
                    VKENG_INFO("Created btConvexHullShape with {} points.", m_Settings.physicsVertices.size());
                }
                break;

            case CollisionShapeType::TRIANGLE_MESH:
                VKENG_INFO("RigidBody '{}': Creating btBvhTriangleMeshShape...", gameObjectName);
                if (m_Settings.physicsVertices.empty() || m_Settings.physicsIndices.empty() || (m_Settings.physicsIndices.size() % 3 != 0)) {
                    VKENG_ERROR("TriangleMesh shape requested for '{}' but physics geometry in settings is invalid (Verts: {}, Idxs: {})! Creating default box.",
                                gameObjectName, m_Settings.physicsVertices.size(), m_Settings.physicsIndices.size());
                    m_Shape = std::make_unique<btBoxShape>(btVector3(0.5f, 0.5f, 0.5f));
                } else {
                    // Note: The btTriangleIndexVertexArray is dynamically allocated here.
                    // btBvhTriangleMeshShape (or btTriangleMeshShape) takes ownership and will delete it.
                    btTriangleIndexVertexArray* meshInterface = new btTriangleIndexVertexArray(
                        static_cast<int>(m_Settings.physicsIndices.size() / 3), // numTriangles
                        (int*)m_Settings.physicsIndices.data(),                 // triangleIndexBase
                        sizeof(uint32_t) * 3,                                   // triangleIndexStride
                        static_cast<int>(m_Settings.physicsVertices.size()),    // numVertices
                        (btScalar*)m_Settings.physicsVertices.data(),           // vertexBase
                        sizeof(glm::vec3)                                       // vertexStride
                    );
                    bool useQuantizedAabbCompression = true; // Generally recommended for performance
                    m_Shape = std::make_unique<btBvhTriangleMeshShape>(meshInterface, useQuantizedAabbCompression);
                    // For dynamic triangle meshes (rare, usually for deforming soft bodies), use btGImpactMeshShape
                    // or update btBvhTriangleMeshShape's AABB if vertices change.
                    VKENG_INFO("Created btBvhTriangleMeshShape ({} triangles, {} vertices).",
                               m_Settings.physicsIndices.size() / 3, m_Settings.physicsVertices.size());
                }
                break;

            default:
                VKENG_ERROR("RigidBody '{}': Unsupported collision shape type requested! Creating default box.", gameObjectName);
                m_Shape = std::make_unique<btBoxShape>(btVector3(0.5f, 0.5f, 0.5f)); // Fallback
                break;
        }
    }

    void RigidBodyComponent::InitializePhysics(PhysicsSystem* physicsSystem) {
        if (m_IsInitialized) {
            VKENG_WARN("RigidBodyComponent for '{}' already initialized. Skipping.", m_GameObject ? m_GameObject->GetName() : "UNKNOWN");
            return;
        }
        if (!physicsSystem) {
            VKENG_ERROR("RigidBodyComponent::InitializePhysics: PhysicsSystem is null for '{}'.", m_GameObject ? m_GameObject->GetName() : "UNKNOWN");
            return;
        }
        if (!m_GameObject) {
            VKENG_ERROR("RigidBodyComponent::InitializePhysics: Owning GameObject is null. Cannot initialize.");
            return;
        }

        m_CachedTransformComponent = m_GameObject->GetComponent<TransformComponent>();
        if (!m_CachedTransformComponent) {
            VKENG_ERROR("RigidBodyComponent on GameObject '{}' requires a TransformComponent but none found! Cannot initialize physics.", m_GameObject->GetName());
            return;
        }

        VKENG_INFO("Initializing physics for RigidBodyComponent on '{}'...", m_GameObject->GetName());

        CreateShape();
        if (!m_Shape) { // Should have created a fallback shape even on error
            VKENG_CRITICAL("RigidBodyComponent::InitializePhysics: Failed to create any collision shape for '{}'.", m_GameObject->GetName());
            return;
        }

        m_MotionState = std::make_unique<EngineMotionState>(m_CachedTransformComponent);

        btScalar mass = (m_Settings.isKinematic || m_Settings.mass <= 0.0f) ? 0.0f : m_Settings.mass;
        btVector3 localInertia(0, 0, 0);
        if (mass > 0.0f) { // Dynamic objects have inertia
            m_Shape->calculateLocalInertia(mass, localInertia);
        }

        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, m_MotionState.get(), m_Shape.get(), localInertia);
        rbInfo.m_friction = m_Settings.friction;
        rbInfo.m_restitution = m_Settings.restitution;
        rbInfo.m_linearDamping = m_Settings.linearDamping;
        rbInfo.m_angularDamping = m_Settings.angularDamping;
        // TODO: rbInfo.m_collisionFlags, m_userIndex, etc.

        m_RigidBody = std::make_unique<btRigidBody>(rbInfo);
        m_RigidBody->setUserPointer(m_GameObject); // Link back to our GameObject

        // Set collision flags based on mass and kinematic setting
        if (m_Settings.isKinematic) { // Mass must be 0 for kinematic as per Bullet docs
            m_RigidBody->setCollisionFlags(m_RigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            m_RigidBody->setActivationState(DISABLE_DEACTIVATION); // Kinematic objects are always "active"
        } else if (mass == 0.0f) { // Static object
            m_RigidBody->setCollisionFlags(m_RigidBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
        }
        // Dynamic objects (mass > 0, not kinematic) have no special flags here by default.

        // Optional: Add to a specific collision filter group and mask
        // physicsSystem->GetWorld()->addRigidBody(m_RigidBody.get(), m_Settings.collisionGroup, m_Settings.collisionMask);
        physicsSystem->AddRigidBody(m_RigidBody.get());

        m_IsInitialized = true;
        VKENG_INFO("RigidBodyComponent for '{}' physics initialized and added to world.", m_GameObject->GetName());
    }

    void RigidBodyComponent::CleanupPhysics(PhysicsSystem* physicsSystem) {
        if (!m_IsInitialized) return;
        if (!physicsSystem) {
            VKENG_WARN("RigidBodyComponent::CleanupPhysics: PhysicsSystem is null for '{}'. Cannot remove body.", m_GameObject ? m_GameObject->GetName() : "UNKNOWN");
            // Proceed to reset local members if possible
        }

        VKENG_INFO("Cleaning up physics for RigidBodyComponent on '{}'...", m_GameObject ? m_GameObject->GetName() : "UNKNOWN");

        if (m_RigidBody && physicsSystem) {
            physicsSystem->RemoveRigidBody(m_RigidBody.get());
        }

        // unique_ptr will handle deletion when reset
        m_RigidBody.reset();
        m_MotionState.reset();
        m_Shape.reset(); // Deletes the btCollisionShape (and btTriangleIndexVertexArray if it was a triangle mesh)

        m_CachedTransformComponent = nullptr;
        m_IsInitialized = false;
        VKENG_INFO("RigidBodyComponent for '{}' physics cleaned up.", m_GameObject ? m_GameObject->GetName() : "UNKNOWN");
    }

    void RigidBodyComponent::Update(float deltaTime) {
        // Component::Update(deltaTime); // Call base if it does something

        // This method is called by GameObject::UpdateComponents AFTER the physics step.
        // The EngineMotionState will have already updated this GameObject's TransformComponent
        // with the new position/rotation from the physics simulation.

        // Use this for logic that needs to run based on the physics state, e.g.:
        // if (m_IsInitialized && m_RigidBody) {
        //     if (!m_RigidBody->isActive()) {
        //         // Object is sleeping
        //     }
        //     // Check velocity, apply status effects, etc.
        // }
    }

    void RigidBodyComponent::OnCollisionEnter(GameObject* otherObject, const btPersistentManifold* manifold) {
        if (!m_GameObject || !otherObject) return;
        VKENG_INFO("Collision ENTER: '{}' with '{}'", m_GameObject->GetName(), otherObject->GetName());
        // Access manifold for contact points: manifold->getContactPoint(i).getPositionWorldOnA() etc.
    }

    void RigidBodyComponent::OnCollisionExit(GameObject* otherObject) {
        if (!m_GameObject || !otherObject) return;
        VKENG_INFO("Collision EXIT: '{}' with '{}'", m_GameObject->GetName(), otherObject->GetName());
    }

    // --- Example Physics Interaction Methods (Implement if needed) ---
    /*
    void RigidBodyComponent::ApplyForce(const glm::vec3& force, const glm::vec3& relativePos) {
        if (m_RigidBody && m_IsInitialized && m_Settings.mass > 0.0f) {
            m_RigidBody->activate(true); // Wake up the body
            m_RigidBody->applyForce(btVector3(force.x, force.y, force.z),
                                    btVector3(relativePos.x, relativePos.y, relativePos.z));
        }
    }

    void RigidBodyComponent::ApplyImpulse(const glm::vec3& impulse, const glm::vec3& relativePos) {
        if (m_RigidBody && m_IsInitialized && m_Settings.mass > 0.0f) {
            m_RigidBody->activate(true);
            m_RigidBody->applyImpulse(btVector3(impulse.x, impulse.y, impulse.z),
                                      btVector3(relativePos.x, relativePos.y, relativePos.z));
        }
    }

    void RigidBodyComponent::SetLinearVelocity(const glm::vec3& velocity) {
        if (m_RigidBody && m_IsInitialized) {
            m_RigidBody->activate(true);
            m_RigidBody->setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
        }
    }
    // ... other getters/setters for velocity, angular factor, etc. ...
    */

} // namespace VulkEng

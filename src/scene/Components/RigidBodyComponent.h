#pragma once

#include "scene/Component.h"                 // Base class for components
#include "physics/CollisionCallbackReceiver.h" // Interface for collision callbacks

#include <glm/glm.hpp> // For glm::vec3 in RigidBodySettings
#include <vector>      // For physicsVertices/Indices in RigidBodySettings
#include <memory>      // For std::unique_ptr to manage Bullet objects

// --- Forward Declare Bullet Types ---
// To avoid including heavy Bullet headers in this component header.
class btRigidBody;
class btCollisionShape;
class btMotionState;
class btPersistentManifold; // For OnCollisionEnter
// --- End Bullet Forward Declarations ---

namespace VulkEng {

    // Forward declarations from our engine
    class PhysicsSystem;      // Needed for adding/removing the rigid body
    class TransformComponent; // Needed for synchronization via motion state

    // Enum to specify the type of collision shape to create for the rigid body.
    enum class CollisionShapeType {
        BOX,
        SPHERE,
        CAPSULE,
        CYLINDER,
        CONVEX_HULL,     // For dynamic complex shapes (built from a point cloud)
        TRIANGLE_MESH,   // For static complex environment meshes
        // PLANE            // btStaticPlaneShape
    };

    // Structure to hold initial settings for creating a RigidBodyComponent.
    struct RigidBodySettings {
        float mass = 1.0f;                     // Mass of the object (0.0f for static or kinematic objects)
        CollisionShapeType shapeType = CollisionShapeType::BOX; // Default to a box shape
        glm::vec3 dimensions = glm::vec3(1.0f); // Interpretation depends on shapeType:
                                               // BOX: half-extents (width/2, height/2, depth/2)
                                               // SPHERE: radius (dimensions.x)
                                               // CAPSULE: radius (dimensions.x), height (dimensions.y)
                                               // CYLINDER: radius (dimensions.x), height (dimensions.y), up-axis often Y
        bool isKinematic = false;              // If true, object is moved by code, not physics forces (mass should be 0)
                                               // If false and mass is 0, it's a static object.
                                               // If false and mass > 0, it's a dynamic object.

        // For TriangleMesh or ConvexHull shapes, provide the geometry:
        std::vector<glm::vec3> physicsVertices;
        std::vector<uint32_t> physicsIndices;

        // Optional physics properties (can be set on btRigidBody directly after creation too)
        float friction = 0.5f;
        float restitution = 0.1f; // Bounciness
        float linearDamping = 0.05f;
        float angularDamping = 0.05f;

        // Collision filtering (optional, advanced)
        // short collisionGroup = 1; // Default group
        // short collisionMask = -1; // Collides with everything (-1 or 0xFFFF)
    };


    // Component that adds physics simulation capabilities to a GameObject
    // using the Bullet Physics library.
    class RigidBodyComponent : public Component, public ICollisionCallbackReceiver {
    public:
        // Constructor takes the initial settings for the rigid body.
        explicit RigidBodyComponent(const RigidBodySettings& settings);
        // Virtual destructor ensures proper cleanup of Bullet objects.
        virtual ~RigidBodyComponent();

        // --- Component Lifecycle ---
        // Called by GameObject when attached (or by user code after attachment).
        // This is where the Bullet rigid body is created and added to the physics world.
        // Requires the owning GameObject to have a TransformComponent.
        void InitializePhysics(PhysicsSystem* physicsSystem);

        // Called by GameObject when detached (or by user code before detachment/destruction).
        // Removes the Bullet rigid body from the physics world and cleans up Bullet objects.
        void CleanupPhysics(PhysicsSystem* physicsSystem);

        // Standard component update method (called by GameObject::UpdateComponents).
        void Update(float deltaTime) override;


        // --- ICollisionCallbackReceiver Implementation ---
        // Called by the physics system when a collision starts.
        void OnCollisionEnter(GameObject* otherObject, const btPersistentManifold* manifold) override;
        // Called by the physics system when a collision ends.
        void OnCollisionExit(GameObject* otherObject) override;


        // --- Accessors & Modifiers ---
        btRigidBody* GetBulletRigidBody() const { return m_RigidBody.get(); } // Be careful with external modification
        bool IsInitialized() const { return m_IsInitialized; }
        const RigidBodySettings& GetSettings() const { return m_Settings; }

        // Add methods to apply forces, impulses, set velocity, etc.
        // Example:
        // void ApplyForce(const glm::vec3& force, const glm::vec3& relativePos = glm::vec3(0.0f));
        // void ApplyImpulse(const glm::vec3& impulse, const glm::vec3& relativePos = glm::vec3(0.0f));
        // void SetLinearVelocity(const glm::vec3& velocity);
        // glm::vec3 GetLinearVelocity() const;
        // void SetAngularVelocity(const glm::vec3& angularVelocity);
        // glm::vec3 GetAngularVelocity() const;
        // void Activate(); // Wakes up the rigid body if it's sleeping


    private:
        // Helper method to create the appropriate btCollisionShape based on m_Settings.
        void CreateShape();

        RigidBodySettings m_Settings;                       // Initial settings for the rigid body
        TransformComponent* m_CachedTransformComponent = nullptr; // Cached pointer to the owning GO's transform

        // Bullet Physics objects, managed by unique_ptr for automatic cleanup.
        std::unique_ptr<btCollisionShape> m_Shape;
        std::unique_ptr<btMotionState> m_MotionState;     // Links Bullet transform with engine transform
        std::unique_ptr<btRigidBody> m_RigidBody;

        bool m_IsInitialized = false; // Tracks if InitializePhysics has been called
    };

} // namespace VulkEng

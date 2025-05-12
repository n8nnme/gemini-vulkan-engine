#pragma once

#include <memory>    // For std::unique_ptr
#include <vector>    // Potentially for tracking bodies, though Bullet world does this
#include <cstdint>   // For uint types if needed

// --- Forward Declare Bullet Types ---
// This avoids including heavy Bullet headers in this public engine header.
// The .cpp file will include the actual Bullet headers.
class btBroadphaseInterface;
class btCollisionDispatcher;
class btConstraintSolver;
class btDefaultCollisionConfiguration;
class btDiscreteDynamicsWorld; // Or btDynamicsWorld if using a different world type
class btRigidBody;
class btInternalTickCallback; // For custom tick callback
// --- End Bullet Forward Declarations ---

namespace VulkEng {

    // Forward declaration for custom tick callback
    class CustomInternalTickCallback;

    // Manages the Bullet Physics world, including its configuration,
    // stepping the simulation, and adding/removing rigid bodies.
    class PhysicsSystem {
    public:
        // Constructor initializes the Bullet physics world.
        // Takes an optional skipInit for Null object pattern.
        PhysicsSystem(bool skipInit = false);
        // Destructor cleans up all Bullet objects.
        // Made virtual for potential inheritance (e.g., NullPhysicsSystem).
        virtual ~PhysicsSystem();

        // Prevent copying and assignment.
        PhysicsSystem(const PhysicsSystem&) = delete;
        PhysicsSystem& operator=(const PhysicsSystem&) = delete;

        // --- Simulation ---
        // Steps the physics simulation forward by `deltaTime`.
        // - deltaTime: The real time elapsed since the last frame.
        // - maxSubSteps: Maximum number of simulation substeps Bullet can take if deltaTime is large.
        // - fixedTimeStep: The desired fixed timestep for the physics simulation (e.g., 1/60th of a second).
        virtual void Update(float deltaTime, int maxSubSteps = 10, float fixedTimeStep = 1.0f / 60.0f);

        // --- Rigid Body Management ---
        // Adds a pre-configured btRigidBody to the physics world.
        // The PhysicsSystem does not take ownership of the btRigidBody pointer itself;
        // that's typically managed by a RigidBodyComponent via unique_ptr.
        virtual void AddRigidBody(btRigidBody* body);
        // Removes a btRigidBody from the physics world.
        virtual void RemoveRigidBody(btRigidBody* body);

        // --- Accessors ---
        // Provides access to the underlying Bullet dynamics world.
        // Be cautious with direct manipulation of the world.
        virtual btDynamicsWorld* GetWorld() const { return m_DynamicsWorld.get(); }

        // Optional: Raycasting, collision queries, etc.
        // virtual bool Raycast(const glm::vec3& from, const glm::vec3& to, RaycastResult& outResult, short group, short mask);


    private:
        // Order of declaration matters for unique_ptr destruction:
        // World should be destroyed first, then solver, broadphase, dispatcher, configuration.
        // unique_ptr handles this automatically if declared in this order.

        std::unique_ptr<btDefaultCollisionConfiguration> m_CollisionConfiguration;
        std::unique_ptr<btCollisionDispatcher> m_Dispatcher;
        std::unique_ptr<btBroadphaseInterface> m_Broadphase;
        std::unique_ptr<btSequentialImpulseConstraintSolver> m_Solver; // Or other solver type
        std::unique_ptr<btDiscreteDynamicsWorld> m_DynamicsWorld;

        // Custom internal tick callback for collision event processing
        std::unique_ptr<CustomInternalTickCallback> m_TickCallback;

        bool m_IsInitialized = false; // Tracks if Bullet was initialized
    };

} // namespace VulkEng

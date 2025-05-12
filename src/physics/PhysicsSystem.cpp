#include "PhysicsSystem.h"
#include "CustomTickCallback.h" // For our custom collision processing
#include "core/Log.h"           // For logging

// --- Include Actual Bullet Headers ---
// This includes most of what's needed for a basic discrete dynamics world.
#include <btBulletDynamicsCommon.h>
// Specific includes if not covered by the above:
// #include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
// #include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
// #include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
// #include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
// #include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
// --- End Bullet ---

#include <stdexcept> // For std::runtime_error

namespace VulkEng {

    PhysicsSystem::PhysicsSystem(bool skipInit /*= false*/)
        : m_CollisionConfiguration(nullptr),
          m_Dispatcher(nullptr),
          m_Broadphase(nullptr),
          m_Solver(nullptr),
          m_DynamicsWorld(nullptr),
          m_TickCallback(nullptr),
          m_IsInitialized(false)
    {
        if (skipInit) {
            VKENG_WARN("PhysicsSystem: Skipping Bullet Physics Initialization for Dummy/Null instance!");
            return; // Exit early
        }

        VKENG_INFO("PhysicsSystem: Initializing Bullet Physics...");

        // 1. Collision Configuration: Provides default setup for collision algorithms.
        //    Can be customized (e.g., for different GImpact algorithms).
        m_CollisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();

        // 2. Collision Dispatcher: Uses the configuration to dispatch collision checks.
        //    Can be customized for multi-threading or specific algorithm needs.
        m_Dispatcher = std::make_unique<btCollisionDispatcher>(m_CollisionConfiguration.get());

        // 3. Broadphase: Efficiently finds potential collision pairs (AABB checks).
        //    btDbvtBroadphase is a good general-purpose dynamic AABB tree.
        m_Broadphase = std::make_unique<btDbvtBroadphase>();

        // 4. Constraint Solver: Resolves constraints and contacts.
        //    btSequentialImpulseConstraintSolver is a common choice.
        m_Solver = std::make_unique<btSequentialImpulseConstraintSolver>();

        // 5. Dynamics World: The main container for physics objects and simulation.
        //    btDiscreteDynamicsWorld is for rigid body dynamics.
        m_DynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(
            m_Dispatcher.get(),
            m_Broadphase.get(),
            m_Solver.get(),
            m_CollisionConfiguration.get()
        );

        if (!m_DynamicsWorld) { // Should not happen if make_unique succeeds
            throw std::runtime_error("Failed to create Bullet Dynamics World!");
        }

        // Set default gravity (Y-down is common in 3D).
        m_DynamicsWorld->setGravity(btVector3(0, -9.81f, 0));

        // 6. Create and Register Custom Internal Tick Callback
        // This is used for processing collision events after each simulation substep.
        m_TickCallback = std::make_unique<CustomInternalTickCallback>(m_DynamicsWorld.get());
        // The third parameter 'isPreTick' to setInternalTickCallback:
        // - true: Callback happens *before* constraints are solved (pre-tick). Good for modifying forces.
        // - false: Callback happens *after* constraints are solved (post-tick). Good for reading results like contact points.
        // For collision event *detection* based on manifolds, post-tick is usually fine, or even during step.
        // The example CustomTickCallback iterates manifolds, so its timing relative to solver isn't critical for just detection.
        // Let's use post-tick (false) which is more common for reading resolved contacts.
        // Or use pre-tick (true) if we need to modify things before solver.
        // Test shows many examples using pre-tick for contact processing.
        m_DynamicsWorld->setInternalTickCallback(m_TickCallback.get(), static_cast<void*>(this), true); // `true` for pre-tick

        m_IsInitialized = true;
        VKENG_INFO("PhysicsSystem: Bullet Physics World Created and Tick Callback Registered (Gravity: 0, -9.81, 0).");
    }

    PhysicsSystem::~PhysicsSystem() {
        VKENG_INFO("PhysicsSystem: Destroying...");

        // Before destroying the world, remove the internal tick callback to avoid dangling pointers
        // if the callback object (m_TickCallback) is destroyed before the world explicitly clears it.
        if (m_DynamicsWorld && m_TickCallback) {
            m_DynamicsWorld->setInternalTickCallback(nullptr, nullptr, false); // Clear callback
        }
        // unique_ptrs handle deletion in the correct order automatically due to declaration order in .h:
        // m_DynamicsWorld, then m_Solver, m_Broadphase, m_Dispatcher, m_CollisionConfiguration, m_TickCallback.
        // Explicitly resetting for clarity or specific order if needed:
        m_DynamicsWorld.reset();
        m_Solver.reset();
        m_Broadphase.reset();
        m_Dispatcher.reset();
        m_CollisionConfiguration.reset();
        m_TickCallback.reset();

        VKENG_INFO("PhysicsSystem: Destroyed.");
    }

    void PhysicsSystem::Update(float deltaTime, int maxSubSteps /*= 10*/, float fixedTimeStep /*= 1.0f / 60.0f*/) {
        if (!m_IsInitialized || !m_DynamicsWorld || !m_TickCallback) {
            // VKENG_WARN_ONCE("PhysicsSystem::Update called but system is not initialized.");
            return;
        }

        // Prepare the tick callback state *before* stepping the simulation.
        // This moves the previous tick's collision pairs to allow for new/exit detection.
        m_TickCallback->PrepareTick();

        // Step the simulation.
        // deltaTime: Real time passed since the last frame.
        // maxSubSteps: Maximum number of substeps Bullet will take if deltaTime is larger than fixedTimeStep.
        //              Prevents physics from "exploding" if frame rate drops significantly.
        // fixedTimeStep: The duration of each internal physics simulation step.
        int numSteps = m_DynamicsWorld->stepSimulation(deltaTime, maxSubSteps, fixedTimeStep);
        // if (numSteps > 0) {
        //     VKENG_TRACE("Physics world stepped {} times.", numSteps); // Can be noisy
        // }
    }

    void PhysicsSystem::AddRigidBody(btRigidBody* body) {
        if (!m_IsInitialized || !m_DynamicsWorld) {
            VKENG_WARN("PhysicsSystem::AddRigidBody: System not initialized. Cannot add body.");
            return;
        }
        if (body) {
            m_DynamicsWorld->addRigidBody(body);
            // VKENG_TRACE("PhysicsSystem: Added RigidBody (UserPtr: {}) to physics world.", body->getUserPointer());
        } else {
            VKENG_WARN("PhysicsSystem::AddRigidBody: Attempted to add a null btRigidBody.");
        }
    }

    void PhysicsSystem::RemoveRigidBody(btRigidBody* body) {
        if (!m_IsInitialized || !m_DynamicsWorld) {
            // VKENG_WARN("PhysicsSystem::RemoveRigidBody: System not initialized. Cannot remove body.");
            return;
        }
        if (body) {
            m_DynamicsWorld->removeRigidBody(body);
            // VKENG_TRACE("PhysicsSystem: Removed RigidBody (UserPtr: {}) from physics world.", body->getUserPointer());
        } else {
            // VKENG_WARN("PhysicsSystem::RemoveRigidBody: Attempted to remove a null btRigidBody.");
        }
    }

} // namespace VulkEng

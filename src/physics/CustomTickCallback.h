#pragma once

// --- Bullet Headers ---
// We need the definition of btInternalTickCallback for inheritance,
// and btDynamicsWorld to store a pointer to it.
#include <LinearMath/btScalar.h> // For btScalar
// It's generally fine to include specific Bullet headers here if they are small
// or if forward declaration is too cumbersome for base classes.
// btDynamicsWorld is a core class, often included.
#include <BulletDynamics/Dynamics/btDynamicsWorld.h> // For btDynamicsWorld and btInternalTickCallback declaration
// --- End Bullet ---

#include <set>      // For std::set to track active collision pairs
#include <utility>  // For std::pair to represent a collision pair

namespace VulkEng {

    // Forward declare Bullet types that are only used as opaque pointers in the set,
    // if we didn't include btDynamicsWorld.h directly.
    // class btCollisionObject; // Already implicitly included via btDynamicsWorld.h

    // Custom internal tick callback class for Bullet Physics.
    // This allows us to inject custom logic into Bullet's simulation loop,
    // specifically for processing collision manifolds to detect enter/stay/exit events.
    class CustomInternalTickCallback : public btInternalTickCallback {
    public:
        // Constructor:
        // - world: A non-owning pointer to the btDynamicsWorld this callback will operate on.
        //          The world's dispatcher is used to access collision manifolds.
        explicit CustomInternalTickCallback(btDynamicsWorld* world);
        virtual ~CustomInternalTickCallback() = default;

        // The core callback method called by Bullet Physics during each simulation step
        // (either pre-tick or post-tick, depending on how it's registered).
        // - world: Pointer to the dynamics world (same as m_World).
        // - timeStep: The fixed time step of the current physics simulation substep.
        virtual void internalTick(btDynamicsWorld* world, btScalar timeStep) override;

        // Prepares the callback for a new simulation tick.
        // This should be called by the PhysicsSystem *before* `world->stepSimulation()`.
        // It updates the collision pair state (moves current pairs to previous).
        void PrepareTick();

    private:
        // Define a type for a collision pair using non-owning pointers to Bullet collision objects.
        // The order of objects in the pair is normalized to ensure uniqueness.
        using CollisionPair = std::pair<const btCollisionObject*, const btCollisionObject*>;

        // Helper to normalize a pair of collision objects so that the one with the
        // smaller memory address always comes first. This makes (A,B) and (B,A) the same key.
        static CollisionPair NormalizePair(const btCollisionObject* objA, const btCollisionObject* objB) {
            return (objA < objB) ? std::make_pair(objA, objB) : std::make_pair(objB, objA);
        }

        btDynamicsWorld* m_World = nullptr; // Non-owning pointer to the dynamics world

        // --- Collision State Tracking ---
        // Set of collision pairs that were active in the *previous* physics tick.
        std::set<CollisionPair> m_PreviousTickActivePairs;
        // Set of collision pairs that are active in the *current* physics tick (built during internalTick).
        std::set<CollisionPair> m_CurrentTickActivePairs;
    };

} // namespace VulkEng

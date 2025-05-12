#pragma once

// --- Forward Declare Bullet Types ---
// To avoid including Bullet headers in this widely-used interface header.
// Implementations (like RigidBodyComponent) will include the full Bullet headers.
class btCollisionObject;    // Represents a body involved in a collision.
class btPersistentManifold; // Contains detailed information about contact points.
// --- End Bullet Forward Declarations ---

namespace VulkEng {

    // Forward declaration of our engine's GameObject class.
    class GameObject;

    // Interface class for objects that want to receive collision event callbacks
    // from the physics system. Components like RigidBodyComponent can implement this
    // to react to collisions.
    class ICollisionCallbackReceiver {
    public:
        // Virtual destructor is essential for interfaces to ensure proper cleanup
        // of derived class instances when deleted via a base interface pointer.
        virtual ~ICollisionCallbackReceiver() = default;

        // --- Collision Event Callbacks ---

        // Called when a new collision contact begins between this object's rigid body
        // and another object's rigid body.
        // - otherObject: A pointer to the OTHER GameObject involved in the collision.
        // - manifold: A pointer to the Bullet `btPersistentManifold` containing detailed
        //             contact point information (e.g., contact points, normals).
        //             This pointer is only valid for the duration of this callback.
        virtual void OnCollisionEnter(GameObject* otherObject, const btPersistentManifold* manifold) = 0;

        // Called when a collision that was previously occurring has ended
        // (i.e., the objects are no longer in contact).
        // - otherObject: A pointer to the OTHER GameObject that is no longer colliding.
        virtual void OnCollisionExit(GameObject* otherObject) = 0;

        // Optional: Called for every physics tick that two objects remain in contact,
        // after OnCollisionEnter has been called and before OnCollisionExit.
        // This can be frequent, so use judiciously.
        // virtual void OnCollisionStay(GameObject* otherObject, const btPersistentManifold* manifold) = 0;
    };

} // namespace VulkEng

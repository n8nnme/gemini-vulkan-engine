#pragma once

#include "scene/Components/TransformComponent.h" // For direct access to engine's transform

#include <LinearMath/btMotionState.h>   // Bullet's base motion state class
#include <LinearMath/btTransform.h>     // For btTransform
#include <LinearMath/btQuaternion.h>  // For btQuaternion
#include <LinearMath/btVector3.h>       // For btVector3

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp> // For glm::quat
#include <glm/gtc/type_ptr.hpp>   // For glm::value_ptr (not strictly needed here but often used with GLM)

#include "core/Log.h" // For optional logging

namespace VulkEng {

    // Custom MotionState for Bullet Physics.
    // This class acts as a bridge between Bullet's internal transform representation
    // and our engine's TransformComponent.
    //
    // How it works:
    // 1. When Bullet needs the initial or current world transform of a rigid body,
    //    it calls `getWorldTransform()`. Our implementation reads from the engine's
    //    TransformComponent.
    // 2. After Bullet simulates physics and updates a rigid body's transform,
    //    it calls `setWorldTransform()`. Our implementation updates the engine's
    //    TransformComponent with the new transform from Bullet.
    class EngineMotionState : public btMotionState {
    public:
        // Constructor:
        // - transformComponent: A raw pointer to the engine's TransformComponent that this
        //                       motion state will synchronize with.
        // - initialTransform: (Optional) An initial transform to set in Bullet if the
        //                     TransformComponent isn't immediately available or its state is preferred.
        EngineMotionState(TransformComponent* transformComponent, const btTransform& initialTransform = btTransform::getIdentity())
            : m_EngineTransformComponent(transformComponent) {
            if (m_EngineTransformComponent) {
                // If a transform component is provided, initialize Bullet's transform from it.
                const glm::vec3& pos = m_EngineTransformComponent->GetPosition();
                const glm::quat& rot = m_EngineTransformComponent->GetRotation();

                m_BulletTransform.setOrigin(btVector3(pos.x, pos.y, pos.z));
                m_BulletTransform.setRotation(btQuaternion(rot.x, rot.y, rot.z, rot.w)); // GLM quat: x,y,z,w
            } else {
                // If no transform component, use the provided initialTransform (or identity).
                m_BulletTransform = initialTransform;
                VKENG_WARN_ONCE("EngineMotionState created without a valid TransformComponent. Using initialBulletTransform or identity.");
            }
        }

        virtual ~EngineMotionState() = default; // Default destructor is fine

        // Called by Bullet to get the current world transform of the rigid body.
        // This should provide the transform from our engine's TransformComponent.
        void getWorldTransform(btTransform& worldTrans) const override {
            // It's possible the TransformComponent was destroyed while the rigid body still exists
            // (though ideally RigidBodyComponent::CleanupPhysics prevents this).
            if (m_EngineTransformComponent) {
                const glm::vec3& pos = m_EngineTransformComponent->GetPosition();
                const glm::quat& rot = m_EngineTransformComponent->GetRotation();

                worldTrans.setOrigin(btVector3(pos.x, pos.y, pos.z));
                worldTrans.setRotation(btQuaternion(rot.x, rot.y, rot.z, rot.w));
            } else {
                // Fallback if our engine transform is gone. Return the last known Bullet transform.
                // This helps prevent crashes if Bullet queries after our component is dead.
                // VKENG_WARN_ONCE("EngineMotionState::getWorldTransform: Engine TransformComponent is null. Returning last known Bullet transform.");
                worldTrans = m_BulletTransform;
            }
        }

        // Called by Bullet after the physics simulation step to update the
        // rigid body's world transform in our engine.
        void setWorldTransform(const btTransform& worldTrans) override {
            // Store the new transform from Bullet.
            m_BulletTransform = worldTrans;

            if (m_EngineTransformComponent) {
                const btQuaternion& rot = worldTrans.getRotation();
                const btVector3& pos = worldTrans.getOrigin();

                // Convert Bullet types back to GLM types and update the TransformComponent.
                // Note: GLM quaternion constructor order is (w, x, y, z).
                // Bullet's btQuaternion getX,getY,getZ,getW.
                m_EngineTransformComponent->SetPosition(glm::vec3(pos.x(), pos.y(), pos.z()));
                m_EngineTransformComponent->SetRotation(glm::quat(rot.w(), rot.x(), rot.y(), rot.z()));
                // The TransformComponent's SetPosition/SetRotation will mark its matrix as dirty.
            }
            // else {
            // VKENG_WARN_ONCE("EngineMotionState::setWorldTransform: Engine TransformComponent is null. Bullet transform updated internally but not synced to engine.");
            // }
        }

        // Helper to explicitly update Bullet's internal transform from the engine's current transform.
        // Useful if the engine transform is manipulated directly and physics needs to be re-synced
        // before the next `getWorldTransform` call (e.g., teleporting an object).
        void SyncToEngineTransform() {
            if (m_EngineTransformComponent) {
                const glm::vec3& pos = m_EngineTransformComponent->GetPosition();
                const glm::quat& rot = m_EngineTransformComponent->GetRotation();
                m_BulletTransform.setOrigin(btVector3(pos.x, pos.y, pos.z));
                m_BulletTransform.setRotation(btQuaternion(rot.x, rot.y, rot.z, rot.w));
            }
        }


    private:
        TransformComponent* m_EngineTransformComponent; // Non-owning pointer to the engine's transform
        btTransform m_BulletTransform;                  // Stores the current transform from Bullet's perspective.
                                                        // Also used as fallback if engine component is gone.
    };

} // namespace VulkEng

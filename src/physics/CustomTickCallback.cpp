#include "CustomTickCallback.h"
#include "scene/GameObject.h"                 // For casting user pointer back to GameObject
#include "scene/Components/RigidBodyComponent.h" // **Need to get this concrete type first**
#include "physics/CollisionCallbackReceiver.h" // The interface for collision callbacks
#include "core/Log.h"                       // For logging collision events

#include <btBulletDynamicsCommon.h> // Includes most common Bullet headers

namespace VulkEng {

    CustomInternalTickCallback::CustomInternalTickCallback(btDynamicsWorld* world)
        : m_World(world) {
        if (!m_World) {
            VKENG_ERROR("CustomInternalTickCallback: Created with a null btDynamicsWorld pointer!");
            // Consider throwing an exception for such a critical failure.
        }
    }

    void CustomInternalTickCallback::PrepareTick() {
        m_PreviousTickActivePairs = std::move(m_CurrentTickActivePairs);
        m_CurrentTickActivePairs.clear();
    }

    void CustomInternalTickCallback::internalTick(btDynamicsWorld* world, btScalar timeStep) {
        if (!m_World || m_World != world) { // Ensure consistency and validity
            VKENG_WARN_ONCE("CustomInternalTickCallback::internalTick: World pointer mismatch or null. Aborting tick processing.");
            return;
        }

        btCollisionDispatcher* dispatcher = m_World->getDispatcher();
        if (!dispatcher) {
            VKENG_WARN_ONCE("CustomInternalTickCallback::internalTick: Null dispatcher. Cannot process collisions.");
            return;
        }

        int numManifolds = dispatcher->getNumManifolds();

        for (int i = 0; i < numManifolds; ++i) {
            btPersistentManifold* contactManifold = dispatcher->getManifoldByIndexInternal(i);
            if (!contactManifold) continue;

            const btCollisionObject* obA_const = contactManifold->getBody0();
            const btCollisionObject* obB_const = contactManifold->getBody1();

            bool hasActualContact = false;
            for (int j = 0; j < contactManifold->getNumContacts(); ++j) {
                if (contactManifold->getContactPoint(j).getDistance() <= 0.0f) {
                    hasActualContact = true;
                    break;
                }
            }
            if (!hasActualContact) continue;

            GameObject* gameObjectA = static_cast<GameObject*>(obA_const->getUserPointer());
            GameObject* gameObjectB = static_cast<GameObject*>(obB_const->getUserPointer());

            if (!gameObjectA || !gameObjectB) {
                VKENG_WARN_ONCE("CustomInternalTickCallback: Collision manifold involves object(s) without valid GameObject user pointer. Skipping.");
                continue;
            }

            CollisionPair currentPair = NormalizePair(obA_const, obB_const);
            m_CurrentTickActivePairs.insert(currentPair);

            if (m_PreviousTickActivePairs.find(currentPair) == m_PreviousTickActivePairs.end()) {
                // --- OnEnter ---
                // Get the RigidBodyComponent, then cast to ICollisionCallbackReceiver
                RigidBodyComponent* rbcA = gameObjectA->GetComponent<RigidBodyComponent>();
                RigidBodyComponent* rbcB = gameObjectB->GetComponent<RigidBodyComponent>();

                ICollisionCallbackReceiver* receiverA = rbcA; // Implicit upcast
                ICollisionCallbackReceiver* receiverB = rbcB; // Implicit upcast

                if (receiverA) {
                    receiverA->OnCollisionEnter(gameObjectB, contactManifold);
                }
                if (receiverB) {
                    // Avoid double-calling if A is B (self-collision, though rare with different user pointers)
                    if (gameObjectA != gameObjectB) { // Check to prevent self-notification for the same event
                        receiverB->OnCollisionEnter(gameObjectA, contactManifold);
                    }
                }
            }
            // else { // Optional: OnStay logic
            //     RigidBodyComponent* rbcA = gameObjectA->GetComponent<RigidBodyComponent>();
            //     RigidBodyComponent* rbcB = gameObjectB->GetComponent<RigidBodyComponent>();
            //     ICollisionCallbackReceiver* receiverA = rbcA;
            //     ICollisionCallbackReceiver* receiverB = rbcB;
            //     if (receiverA /* && receiverA->WantsOnStay() */ ) receiverA->OnCollisionStay(gameObjectB, contactManifold);
            //     if (receiverB /* && receiverB->WantsOnStay() */ && gameObjectA != gameObjectB) receiverB->OnCollisionStay(gameObjectA, contactManifold);
            // }
        }

        // --- Check for Exiting Collisions ---
        std::vector<CollisionPair> exitedPairs;
        for (const auto& prevPair : m_PreviousTickActivePairs) {
            if (m_CurrentTickActivePairs.find(prevPair) == m_CurrentTickActivePairs.end()) {
                exitedPairs.push_back(prevPair);
            }
        }

        for (const auto& exitedPair : exitedPairs) {
            GameObject* gameObjectA = static_cast<GameObject*>(exitedPair.first->getUserPointer());
            GameObject* gameObjectB = static_cast<GameObject*>(exitedPair.second->getUserPointer());

            if (!gameObjectA || !gameObjectB) {
                VKENG_WARN_ONCE("CustomInternalTickCallback: Exited collision involves object(s) that became null. Skipping OnExit.");
                continue;
            }

            RigidBodyComponent* rbcA = gameObjectA->GetComponent<RigidBodyComponent>();
            RigidBodyComponent* rbcB = gameObjectB->GetComponent<RigidBodyComponent>();

            ICollisionCallbackReceiver* receiverA = rbcA;
            ICollisionCallbackReceiver* receiverB = rbcB;

            if (receiverA) {
                receiverA->OnCollisionExit(gameObjectB);
            }
            if (receiverB) {
                 if (gameObjectA != gameObjectB) {
                    receiverB->OnCollisionExit(gameObjectA);
                }
            }
        }
    }
} // namespace VulkEng

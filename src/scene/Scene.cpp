#include "Scene.h"
#include "GameObject.h"
#include "Components/CameraComponent.h"   // For checking if a GO has a camera
#include "Components/TransformComponent.h" // For getting camera transform
#include "core/Log.h"                   // For logging scene events

#include <algorithm> // For std::find_if, std::remove_if

namespace VulkEng {

    Scene::Scene() {
        VKENG_INFO("Scene Created (Instance: {}).", static_cast<void*>(this));
        // OnLoad(); // Optionally call OnLoad here or let the application manage it
    }

    Scene::~Scene() {
        VKENG_INFO("Scene Destroying (Instance: {})...", static_cast<void*>(this));
        OnUnload(); // Call unload logic before clearing objects

        // Ensure all objects marked for destruction are processed
        ProcessDestructionList();

        // Clear GameObjects. unique_ptr destructors will handle individual GameObject cleanup.
        // This will trigger GameObject destructors, which in turn trigger component destruction.
        m_GameObjects.clear();
        m_MainCameraObject = nullptr; // Clear camera reference
        VKENG_INFO("Scene Destroyed.");
    }

    GameObject* Scene::CreateGameObject(const std::string& name /*= "GameObject"*/) {
        // Create a new GameObject and add it to the scene's list.
        // The scene takes ownership via unique_ptr.
        m_GameObjects.emplace_back(std::make_unique<GameObject>(name, this));
        GameObject* newGameObject = m_GameObjects.back().get(); // Get raw pointer to return

        VKENG_INFO("Scene: Created GameObject '{}' (ID: {}).", newGameObject->GetName(), static_cast<void*>(newGameObject));
        return newGameObject;
    }

    GameObject* Scene::FindGameObjectByName(const std::string& name) const {
        for (const auto& go : m_GameObjects) {
            if (go && go->GetName() == name) { // Check for nullptr just in case
                return go.get();
            }
        }
        VKENG_WARN("Scene: GameObject with name '{}' not found.", name);
        return nullptr;
    }

    void Scene::DestroyGameObject(GameObject* gameObject) {
        if (!gameObject) {
            VKENG_WARN("Scene: Attempted to destroy a null GameObject.");
            return;
        }

        // Add to a deferred destruction list to avoid issues if called during iteration (e.g., in Update loop).
        // Check if it's already marked to prevent duplicates.
        if (std::find(m_ObjectsToDestroy.begin(), m_ObjectsToDestroy.end(), gameObject) == m_ObjectsToDestroy.end()) {
            m_ObjectsToDestroy.push_back(gameObject);
            VKENG_INFO("Scene: GameObject '{}' (ID: {}) marked for destruction.", gameObject->GetName(), static_cast<void*>(gameObject));
        }
    }

    void Scene::ProcessDestructionList() {
        if (m_ObjectsToDestroy.empty()) {
            return;
        }

        VKENG_INFO("Scene: Processing {} GameObjects for destruction...", m_ObjectsToDestroy.size());
        for (GameObject* goPtr : m_ObjectsToDestroy) {
            // Find and remove the GameObject from the main list.
            // std::remove_if moves elements to be erased to the end and returns an iterator to the new end.
            auto it = std::remove_if(m_GameObjects.begin(), m_GameObjects.end(),
                                     [goPtr](const std::unique_ptr<GameObject>& uPtr) {
                                         return uPtr.get() == goPtr;
                                     });

            if (it != m_GameObjects.end()) {
                 // Actual erasure (unique_ptr destructor will be called)
                 // We can iterate and erase unique_ptrs directly if we are careful or use erase-remove idiom.
                 // For simplicity here, we'll just erase the found range.
                 // Note: This loop might erase multiple if pointers were duplicated in m_ObjectsToDestroy (shouldn't happen).
                 VKENG_INFO("Scene: Actually destroying GameObject '{}' (ID: {}).", goPtr->GetName(), static_cast<void*>(goPtr));
                 m_GameObjects.erase(it, m_GameObjects.end()); // Erase elements from 'it' to end
            } else {
                 VKENG_WARN("Scene: GameObject marked for destruction was not found in the main list (already destroyed?).");
            }

            // If the destroyed object was the main camera, nullify the reference.
            if (m_MainCameraObject == goPtr) {
                m_MainCameraObject = nullptr;
                VKENG_INFO("Scene: Main camera GameObject was destroyed.");
            }
        }
        m_ObjectsToDestroy.clear(); // Clear the list after processing
    }


    void Scene::Update(float deltaTime) {
        // 1. Process deferred destruction first to remove objects before they are updated.
        ProcessDestructionList();

        // 2. Update Camera View Matrix (if camera exists and has transform)
        if (m_MainCameraObject) {
            CameraComponent* camComponent = m_MainCameraObject->GetComponent<CameraComponent>();
            TransformComponent* camTransform = m_MainCameraObject->GetComponent<TransformComponent>();
            if (camComponent && camTransform) {
                camComponent->UpdateViewMatrix(*camTransform); // Camera updates its view based on its transform
            }
        }

        // 3. Update all active GameObjects and their components.
        // Iterate by index to allow safe removal if ProcessDestructionList wasn't perfectly safe or if components self-destruct.
        // However, direct removal during this loop is generally unsafe. Deferred destruction is better.
        for (const auto& go : m_GameObjects) {
            if (go) { // Check if unique_ptr is not null (should always be true if managed correctly)
                // Call the GameObject's own update loop, which will update its components.
                go->UpdateComponents(deltaTime);
            }
        }
    }

    void Scene::SetMainCamera(GameObject* cameraObject) {
        // Basic validation: check if the provided GameObject has a CameraComponent and TransformComponent.
        if (cameraObject &&
            cameraObject->GetComponent<CameraComponent>() &&
            cameraObject->GetComponent<TransformComponent>()) {
            m_MainCameraObject = cameraObject;
            VKENG_INFO("Scene: Main camera set to GameObject '{}'.", cameraObject->GetName());
        } else {
            // m_MainCameraObject = nullptr; // Or keep previous? Clear for safety.
            VKENG_WARN("Scene: Attempted to set main camera to GameObject '{}' which lacks CameraComponent or TransformComponent.",
                       cameraObject ? cameraObject->GetName() : "nullptr");
        }
    }

    CameraComponent* Scene::GetMainCamera() const {
        if (m_MainCameraObject) {
            return m_MainCameraObject->GetComponent<CameraComponent>();
        }
        return nullptr;
    }

    TransformComponent* Scene::GetMainCameraTransform() const {
        if (m_MainCameraObject) {
            return m_MainCameraObject->GetComponent<TransformComponent>();
        }
        return nullptr;
    }

} // namespace VulkEng

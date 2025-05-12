#pragma once

#include <vector>
#include <string>
#include <memory>    // For std::unique_ptr
#include <cstdint>   // For uint32_t (if using for entity IDs with an ECS)
#include <algorithm> // For std::remove_if

// Forward Declarations to avoid circular dependencies or heavy includes
namespace VulkEng {
    class GameObject;       // GameObjects are managed by the Scene
    class CameraComponent;  // A Scene typically has a main camera
    class TransformComponent; // Often needed for camera transform access
    // If using an Entity-Component-System (ECS) library like EnTT:
    // #include <entt/entt.hpp>
}

namespace VulkEng {

    // Represents a collection of GameObjects and manages their lifecycle and updates.
    // A game can have multiple scenes (e.g., main menu, level 1, level 2).
    class Scene {
    public:
        Scene();
        ~Scene();

        // Prevent copying and assignment. Scenes are unique.
        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;

        // --- GameObject Management ---
        // Creates a new GameObject within this scene.
        // Returns a raw pointer to the created GameObject (owned by the scene).
        GameObject* CreateGameObject(const std::string& name = "GameObject");

        // Finds a GameObject by its name (can be slow for many objects).
        GameObject* FindGameObjectByName(const std::string& name) const;

        // Destroys a GameObject. This will also trigger OnDetach for its components.
        // This needs careful implementation to handle dependencies and iteration.
        void DestroyGameObject(GameObject* gameObject);
        // void DestroyGameObject(EntityID entity); // If using an ECS


        // --- Scene Lifecycle ---
        // Called once when the scene is loaded or becomes active.
        virtual void OnLoad() { /* User-defined load logic */ }
        // Called every frame to update the scene's state and its GameObjects.
        virtual void Update(float deltaTime);
        // Called once when the scene is about to be unloaded or deactivated.
        virtual void OnUnload() { /* User-defined unload logic */ }


        // --- Camera Management ---
        // Sets the primary camera used for rendering this scene.
        void SetMainCamera(GameObject* cameraObject);
        // Gets the main camera component (can be nullptr).
        CameraComponent* GetMainCamera() const;
        // Helper to get the transform of the main camera (can be nullptr).
        TransformComponent* GetMainCameraTransform() const;


        // --- Accessing GameObjects (for rendering, physics, etc.) ---
        // Provides access to all GameObjects in the scene.
        // Note: Iterating this vector directly can be problematic if GameObjects are destroyed during iteration.
        // Consider providing iterators or a safer access method.
        const std::vector<std::unique_ptr<GameObject>>& GetAllGameObjects() const {
            return m_GameObjects;
        }

        // If using an ECS like EnTT:
        // entt::registry& GetRegistry() { return m_Registry; }
        // const entt::registry& GetRegistry() const { return m_Registry; }

    private:
        // Storage for GameObjects. Using unique_ptr ensures they are automatically
        // deleted when the scene is destroyed or when explicitly removed.
        std::vector<std::unique_ptr<GameObject>> m_GameObjects;
        // For efficient removal, consider std::list or mark-and-sweep if frequent deletions.

        // Pointer to the GameObject designated as the main camera.
        // This is a raw pointer; the GameObject itself is owned by m_GameObjects.
        GameObject* m_MainCameraObject = nullptr;

        // Flag to mark objects for deferred destruction to avoid iterator invalidation issues.
        std::vector<GameObject*> m_ObjectsToDestroy;
        void ProcessDestructionList();


        // If using an Entity-Component-System (ECS) like EnTT:
        // entt::registry m_Registry;
    };

} // namespace VulkEng

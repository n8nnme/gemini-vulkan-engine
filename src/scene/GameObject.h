#pragma once

#include "Component.h" // Base class for all components
#include "core/Log.h"    // For logging component operations

#include <string>
#include <vector>
#include <memory>        // For std::unique_ptr to manage component ownership
#include <unordered_map> // For efficient component lookup by type
#include <typeindex>     // For using type_info as map keys
#include <stdexcept>     // For std::runtime_error (e.g., if component already exists)
#include <algorithm>     // For std::find_if (potentially)

namespace VulkEng {

    // Forward declaration of Scene to avoid circular dependency
    // GameObject belongs to a Scene.
    class Scene;

    // Represents an entity in the game world.
    // GameObjects are containers for Components, which define their behavior and data.
    class GameObject {
    public:
        // Constructor:
        // - name: A human-readable name for the GameObject (for debugging/identification).
        // - ownerScene: A raw pointer to the Scene that owns this GameObject.
        GameObject(const std::string& name, Scene* ownerScene);
        // Destructor: Handles cleanup, including detaching and destroying components.
        ~GameObject();

        // --- Rule of Five/Six ---
        // GameObjects are unique entities and should not be copied.
        GameObject(const GameObject&) = delete;
        GameObject& operator=(const GameObject&) = delete;
        // Move constructor and assignment can be defaulted if unique_ptr members handle moves correctly.
        GameObject(GameObject&&) noexcept; // Declare custom move for explicit resource handling
        GameObject& operator=(GameObject&&) noexcept;


        // --- Accessors ---
        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }

        Scene* GetScene() const { return m_OwnerScene; } // Get the scene this GameObject belongs to

        // Optional: Active state for the GameObject
        // bool IsActive() const { return m_IsActive; }
        // void SetActive(bool active) { m_IsActive = active; }


        // --- Component Management ---

        // Adds a component of type T to this GameObject.
        // Args... are passed to the component's constructor.
        // Returns a pointer to the newly created and attached component.
        // Throws std::runtime_error or logs a warning if a component of the same type already exists.
        template <typename T, typename... Args>
        T* AddComponent(Args&&... args) {
            // Ensure T is derived from Component at compile time.
            static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");

            std::type_index typeIndex(typeid(T));

            // Check if a component of this type already exists.
            if (m_Components.count(typeIndex)) {
                // Behavior on adding duplicate: replace, error, or return existing?
                // For now, log a warning and replace.
                VKENG_WARN("GameObject '{}': Component of type '{}' already exists. Replacing.", m_Name, typeid(T).name());
                // Explicitly detach and destroy the old one before replacing
                m_Components[typeIndex]->OnDetach(); // Call lifecycle method
                m_Components.erase(typeIndex);
            }

            // Create the new component using perfect forwarding for arguments.
            auto newComponent = std::make_unique<T>(std::forward<Args>(args)...);
            T* rawPtr = newComponent.get(); // Get raw pointer before moving unique_ptr

            // Assign this GameObject as the owner of the component.
            rawPtr->m_GameObject = this;

            // Store the component in the map.
            m_Components[typeIndex] = std::move(newComponent);

            VKENG_INFO("GameObject '{}': Added component '{}'.", m_Name, typeid(T).name());

            // Call the component's OnAttach lifecycle method.
            rawPtr->OnAttach();

            return rawPtr;
        }

        // Retrieves a component of type T attached to this GameObject.
        // Returns nullptr if no component of the specified type is found.
        template <typename T>
        T* GetComponent() const {
            static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");
            std::type_index typeIndex(typeid(T));

            auto it = m_Components.find(typeIndex);
            if (it != m_Components.end() && it->second) {
                // Safely cast the base Component pointer to the derived type T*.
                return static_cast<T*>(it->second.get());
            }
            return nullptr; // Component not found
        }

        // Checks if this GameObject has a component of type T.
        template <typename T>
        bool HasComponent() const {
            static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");
            std::type_index typeIndex(typeid(T));
            return m_Components.count(typeIndex) > 0;
        }

        // Removes a component of type T from this GameObject.
        // If the component exists, its OnDetach method is called before destruction.
        template <typename T>
        void RemoveComponent() {
            static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");
            std::type_index typeIndex(typeid(T));

            auto it = m_Components.find(typeIndex);
            if (it != m_Components.end() && it->second) {
                VKENG_INFO("GameObject '{}': Removing component '{}'.", m_Name, typeid(T).name());
                it->second->OnDetach(); // Call lifecycle method
                m_Components.erase(it); // unique_ptr destructor handles component deletion
            } else {
                VKENG_WARN("GameObject '{}': Attempted to remove non-existent component '{}'.", m_Name, typeid(T).name());
            }
        }

        // --- Update ---
        // Called by the Scene to update all components of this GameObject.
        void UpdateComponents(float deltaTime);

        // Optional: Transform hierarchy
        // GameObject* GetParent() const;
        // void SetParent(GameObject* parent);
        // const std::vector<GameObject*>& GetChildren() const;
        // void AddChild(GameObject* child);
        // void RemoveChild(GameObject* child);


    private:
        std::string m_Name;
        Scene* m_OwnerScene = nullptr; // Non-owning pointer to the scene it belongs to
        // bool m_IsActive = true;     // To enable/disable updates and rendering for this GO

        // Stores components mapped by their std::type_index for efficient lookup.
        // unique_ptr manages the lifetime of the components.
        std::unordered_map<std::type_index, std::unique_ptr<Component>> m_Components;

        // Transform hierarchy members (example)
        // GameObject* m_Parent = nullptr;
        // std::vector<GameObject*> m_Children; // Children do not own their parent, parent might own children (or scene owns all)
    };

} // namespace VulkEng

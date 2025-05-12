#pragma once

#include <string> // For potential component naming or identification

namespace VulkEng {

    // Forward declaration of GameObject to avoid circular dependencies
    class GameObject;

    // Base class for all components that can be attached to a GameObject.
    // Components define behaviors and data associated with GameObjects.
    class Component {
    public:
        // Virtual destructor is crucial for proper cleanup when deleting
        // derived components through a base class pointer.
        virtual ~Component() = default;

        // Gets the GameObject that this component is attached to.
        // Returns nullptr if the component is not attached or has been detached.
        GameObject* GetGameObject() const { return m_GameObject; }

        // --- Core Lifecycle Methods (Optional, to be overridden by derived components) ---

        // Called once when the component is first attached to a GameObject and
        // typically after the GameObject itself is fully constructed and in a scene.
        // Use this for initialization that depends on the GameObject or other components.
        virtual void OnAttach() {
            // Default implementation does nothing.
        }

        // Called when the component is about to be removed from a GameObject,
        // or when the GameObject is being destroyed.
        // Use this for cleanup specific to the component's attachment.
        virtual void OnDetach() {
            // Default implementation does nothing.
        }

        // Called every frame if the GameObject is active and the Scene updates it.
        // `deltaTime` is the time in seconds since the last frame.
        virtual void Update(float deltaTime) {
            // Default implementation does nothing.
            // Derived components override this to implement frame-by-frame logic.
        }

        // Optional: Enable/Disable state for components
        // bool IsEnabled() const { return m_IsEnabled; }
        // virtual void SetEnabled(bool enabled) { m_IsEnabled = enabled; }

    protected:
        // Constructor is protected to prevent direct instantiation of the base Component class.
        // Components should be created as derived types.
        Component() = default;

        // Allow GameObject to set the owner pointer when attaching.
        friend class GameObject;
        GameObject* m_GameObject = nullptr; // Pointer to the owning GameObject

        // bool m_IsEnabled = true; // Optional enabled state
    };

} // namespace VulkEng

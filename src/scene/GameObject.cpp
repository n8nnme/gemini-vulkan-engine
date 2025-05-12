#include "GameObject.h"
#include "Scene.h"    // For potential interaction with the scene (e.g., during destruction)
#include "core/Log.h" // For logging GameObject lifecycle events
#include "Component.h"// For the base Component class (used in UpdateComponents)

#include <utility> // For std::move

namespace VulkEng {

    // --- Constructor ---
    GameObject::GameObject(const std::string& name, Scene* ownerScene)
        : m_Name(name), m_OwnerScene(ownerScene)
          // m_IsActive(true) // Initialize active state if using
    {
        // VKENG_TRACE("GameObject '{}' (ID: {}) constructed in Scene (ID: {}).",
        //             m_Name, static_cast<void*>(this), static_cast<void*>(m_OwnerScene));
    }

    // --- Destructor ---
    GameObject::~GameObject() {
        // VKENG_TRACE("GameObject '{}' (ID: {}) destructing...", m_Name, static_cast<void*>(this));

        // Components are stored as unique_ptrs in m_Components map.
        // When m_Components is cleared (or GameObject is destroyed),
        // the unique_ptrs will automatically delete the Component objects,
        // calling their destructors.
        // It's good practice to call OnDetach for any remaining components explicitly,
        // though unique_ptr destruction handles memory.
        for (auto& pair : m_Components) {
            if (pair.second) {
                pair.second->OnDetach(); // Ensure lifecycle method is called
            }
        }
        m_Components.clear(); // Explicitly clear, though destructor would do it.

        // If managing a transform hierarchy, notify parent/children here.
        // e.g., if (m_Parent) m_Parent->RemoveChild(this);
        // for (auto* child : m_Children) child->SetParent(nullptr); // Orphan children

        // VKENG_TRACE("GameObject '{}' destructed.", m_Name);
    }

    // --- Move Constructor ---
    GameObject::GameObject(GameObject&& other) noexcept
        : m_Name(std::move(other.m_Name)),
          m_OwnerScene(other.m_OwnerScene), // Copy scene pointer
          // m_IsActive(other.m_IsActive),
          m_Components(std::move(other.m_Components)) // Move the component map
          // m_Parent(other.m_Parent), // Move parent
          // m_Children(std::move(other.m_Children)) // Move children
    {
        // VKENG_TRACE("GameObject '{}' move constructed from GameObject '{}'.", m_Name, static_cast<void*>(&other));

        // After moving components, their m_GameObject pointer still points to 'other'.
        // We need to update them to point to 'this' new GameObject.
        for (auto const& [typeIndex, componentPtr] : m_Components) {
            if (componentPtr) {
                componentPtr->m_GameObject = this;
            }
        }

        // Nullify other's pointers to avoid double management if it's not fully destructed.
        other.m_OwnerScene = nullptr;
        // other.m_Parent = nullptr;
        // other.m_Children.clear();
        // The component map in 'other' is now empty due to std::move.
    }

    // --- Move Assignment Operator ---
    GameObject& GameObject::operator=(GameObject&& other) noexcept {
        if (this != &other) { // Prevent self-assignment
            // VKENG_TRACE("GameObject '{}' move assigned from GameObject '{}'.", m_Name, static_cast<void*>(&other));

            // 1. Release current resources (components)
            for (auto& pair : m_Components) {
                if (pair.second) {
                    pair.second->OnDetach();
                }
            }
            m_Components.clear();
            // Handle existing parent/children relationships if any.

            // 2. Steal resources from 'other'
            m_Name = std::move(other.m_Name);
            m_OwnerScene = other.m_OwnerScene;
            // m_IsActive = other.m_IsActive;
            m_Components = std::move(other.m_Components); // Move component map
            // m_Parent = other.m_Parent;
            // m_Children = std::move(other.m_Children);

            // 3. Update m_GameObject pointers in moved components
            for (auto const& [typeIndex, componentPtr] : m_Components) {
                if (componentPtr) {
                    componentPtr->m_GameObject = this;
                }
            }

            // 4. Nullify 'other's relevant members
            other.m_OwnerScene = nullptr;
            // other.m_IsActive = false; // Or some default
            // other.m_Parent = nullptr;
            // other.m_Children.clear();
            // 'other.m_Components' is already empty due to std::move.
        }
        return *this;
    }


    // --- UpdateComponents Implementation ---
    // Called by the Scene to update all components of this GameObject.
    void GameObject::UpdateComponents(float deltaTime) {
        // if (!m_IsActive) return; // Optional: Skip update if GameObject is inactive

        // Iterate through all components attached to this GameObject.
        // Using a range-based for loop on m_Components is generally safe here because
        // components should not be added/removed *during* their own Update or another component's Update
        // on the same GameObject within the same frame. If such re-entrancy is possible,
        // a copy of the component list or more careful iteration would be needed.
        for (auto const& [typeIndex, componentPtr] : m_Components) {
            if (componentPtr /*&& componentPtr->IsEnabled()*/) { // Check if component is valid (and optionally enabled)
                componentPtr->Update(deltaTime); // Call the virtual Update method
            }
        }
    }

    // --- Optional: Transform Hierarchy Method Implementations (Examples) ---
    /*
    GameObject* GameObject::GetParent() const {
        return m_Parent;
    }

    void GameObject::SetParent(GameObject* parent) {
        // TODO: Handle detaching from old parent, attaching to new parent,
        // and updating transform relative to the new parent.
        // This involves complex transform calculations (world to local, local to world).
        if (m_Parent == parent) return;

        if (m_Parent) {
            // m_Parent->RemoveChild(this); // Parent needs a RemoveChild method
        }
        m_Parent = parent;
        if (m_Parent) {
            // m_Parent->AddChild(this); // Parent needs an AddChild method
        }
        // Update local transform based on new parent
    }

    const std::vector<GameObject*>& GameObject::GetChildren() const {
        return m_Children;
    }

    void GameObject::AddChild(GameObject* child) {
        if (child && child->GetParent() != this) {
            // child->SetParent(this); // This would handle logic in SetParent
            // m_Children.push_back(child);
        }
    }

    void GameObject::RemoveChild(GameObject* child) {
        // auto it = std::find(m_Children.begin(), m_Children.end(), child);
        // if (it != m_Children.end()) {
        //     (*it)->SetParent(nullptr); // Or re-parent to this GameObject's parent
        //     m_Children.erase(it);
        // }
    }
    */

} // namespace VulkEng

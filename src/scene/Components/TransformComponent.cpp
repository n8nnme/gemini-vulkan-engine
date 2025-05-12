#include "TransformComponent.h"
#include "scene/GameObject.h" // Optional: If needing to interact with owning GameObject
#include "core/Log.h"         // Optional: For logging specific transform events

namespace VulkEng {

    // --- Constructor (if not defaulted or inlined) ---
    // TransformComponent::TransformComponent() : m_IsDirty(true) {
    //     RecalculateMatrix(); // Ensure matrix is initialized
    //     // VKENG_TRACE("TransformComponent Created for GameObject (if available): {}",
    //     //             m_GameObject ? m_GameObject->GetName() : "UNATTACHED");
    // }

    // --- Destructor (if not defaulted or inlined) ---
    // TransformComponent::~TransformComponent() {
    //     // VKENG_TRACE("TransformComponent Destroyed for GameObject (if available): {}",
    //     //             m_GameObject ? m_GameObject->GetName() : "DETACHED");
    // }


    // --- Optional: Lifecycle Methods Implementation (if declared virtual in header) ---
    // void TransformComponent::OnAttach() {
    //     // Component::OnAttach(); // Call base if it does something
    //     // Logic to perform when this component is attached to a GameObject.
    //     // For example, if a hierarchy exists, update based on parent.
    //     // if (m_GameObject && m_GameObject->GetParent()) {
    //     //     if (auto* parentTransform = m_GameObject->GetParent()->GetComponent<TransformComponent>()) {
    //     //         SetParentTransform(parentTransform); // Hypothetical method
    //     //     }
    //     // }
    //     m_IsDirty = true; // Mark as dirty to recalculate with potential parent context
    // }

    // void TransformComponent::Update(float deltaTime) {
    //     // Component::Update(deltaTime); // Call base if it does something
    //     // Per-frame logic for the transform, if any (e.g., scripted animations, physics updates NOT handled by Bullet).
    //     // For simple PRS transforms, usually no per-frame update is needed here;
    //     // changes are driven by SetPosition/SetRotation/SetScale calls.
    //     // if (m_IsDirty) { // Not usually needed here, GetWorldMatrix handles it
    //     //    RecalculateMatrix();
    //     // }
    // }


    // --- RecalculateMatrix (if moved from header for complexity) ---
    // Note: It was made 'mutable' and called from const getters in the header,
    // which is a valid pattern. If it became extremely complex, it could be non-const
    // and called more explicitly, but the dirty flag pattern is common.
    /*
    void TransformComponent::RecalculateMatrix() { // Would not be const if non-mutable members used
        glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), m_Position);
        glm::mat4 rotationMat = glm::toMat4(m_Rotation);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), m_Scale);

        m_LocalToWorldMatrix = translationMat * rotationMat * scaleMat;

        // If hierarchy exists:
        // if (m_Parent) {
        //     m_LocalToWorldMatrix = m_Parent->GetWorldMatrix() * m_LocalToWorldMatrix;
        // }
        m_IsDirty = false;
    }
    */

    // Currently, all substantive logic for TransformComponent is in the header file
    // (TransformComponent.h) due to inlining of getters/setters and the dirty flag pattern
    // for RecalculateMatrix. This .cpp file is here for completeness and future expansion
    // if methods become too complex for the header.

} // namespace VulkEng

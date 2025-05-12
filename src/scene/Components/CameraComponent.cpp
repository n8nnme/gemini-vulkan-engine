#include "CameraComponent.h"
#include "scene/GameObject.h" // Optional: If OnAttach/OnDetach needed to interact with GameObject
#include "core/Log.h"         // Optional: For logging camera-specific events

namespace VulkEng {

    // --- Constructor (if not defaulted or inlined in header) ---
    // CameraComponent::CameraComponent() {
    //     // Default projection is set up in the header's default member initializers
    //     // or by calling SetPerspective in the header's constructor.
    //     // VKENG_TRACE("CameraComponent Created for GameObject (if available): {}",
    //     //             m_GameObject ? m_GameObject->GetName() : "UNATTACHED");
    // }

    // --- Destructor (if not defaulted or inlined in header) ---
    // CameraComponent::~CameraComponent() {
    //     // VKENG_TRACE("CameraComponent Destroyed for GameObject (if available): {}",
    //     //             m_GameObject ? m_GameObject->GetName() : "DETACHED");
    // }


    // --- Method Implementations (if moved from header) ---
    // SetPerspective, SetOrthographic, SetOrthographicSize, UpdateViewMatrix,
    // RecalculateProjectionMatrix, and getters are currently inline in CameraComponent.h.
    // If any of these become significantly complex, their implementations would be moved here.

    /* Example if UpdateViewMatrix was moved here:
    void CameraComponent::UpdateViewMatrix(const TransformComponent& transform) {
        const glm::vec3& position = transform.GetPosition();
        glm::vec3 target = position + transform.GetForward();
        glm::vec3 up = transform.GetUp();
        m_ViewMatrix = glm::lookAt(position, target, up);
    }
    */

    /* Example if RecalculateProjectionMatrix was moved here:
    void CameraComponent::RecalculateProjectionMatrix() {
        if (m_IsOrthographic) {
            m_ProjectionMatrix = glm::ortho(m_OrthoLeft, m_OrthoRight, m_OrthoBottom, m_OrthoTop, m_NearPlane, m_FarPlane);
        } else {
            m_ProjectionMatrix = glm::perspective(m_FovRadians, m_AspectRatio, m_NearPlane, m_FarPlane);
        }
        m_ProjectionMatrix[1][1] *= -1; // Vulkan Y-flip
    }
    */


    // --- Optional: Component Lifecycle Methods Implementation ---
    // void CameraComponent::OnAttach() {
    //     Component::OnAttach(); // Call base
    //     VKENG_TRACE("CameraComponent OnAttach for GameObject '{}'",
    //                 m_GameObject ? m_GameObject->GetName() : "UNATTACHED");
    //     // Example: Ensure a TransformComponent exists
    //     // if (m_GameObject && !m_GameObject->GetComponent<TransformComponent>()) {
    //     //     VKENG_WARN("CameraComponent on '{}' attached, but no TransformComponent found! Adding one.", m_GameObject->GetName());
    //     //     m_GameObject->AddComponent<TransformComponent>();
    //     // }
    //     // Initial view matrix update might be useful here if transform is set
    //     // if (m_GameObject) {
    //     //    if(auto* tc = m_GameObject->GetComponent<TransformComponent>()) UpdateViewMatrix(*tc);
    //     // }
    // }

    // void CameraComponent::Update(float deltaTime) {
    //     // Component::Update(deltaTime); // Call base
    //     // Per-frame camera logic could go here, e.g.:
    //     // - Camera shake effects
    //     // - Smooth following of a target (if not handled by a separate controller script)
    //     // - Damping camera movements
    //
    //     // For this engine, camera movement is handled in Application::HandleCameraInput
    //     // and the view matrix update is driven by Scene::Update.
    // }


    // This .cpp file primarily serves as a placeholder for non-inline implementations
    // if the methods in CameraComponent.h become more complex.

} // namespace VulkEng

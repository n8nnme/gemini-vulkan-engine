#pragma once

#include "scene/Component.h" // Base class for components
#include "scene/Components/TransformComponent.h" // Often needed to calculate view matrix

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // For glm::lookAt, glm::perspective, glm::ortho
#include <glm/gtc/quaternion.hpp>       // For potential orientation if not using LookAt directly

namespace VulkEng {

    // Forward declaration (TransformComponent is included, but good practice)
    // class TransformComponent;

    // Component that defines a camera's viewing and projection properties.
    // It works in conjunction with a TransformComponent on the same GameObject
    // to determine its position and orientation in the world.
    class CameraComponent : public Component {
    public:
        CameraComponent() {
            // Initialize with default perspective projection
            SetPerspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 1000.0f);
            // View matrix will be calculated based on TransformComponent in Scene::Update
            // or when explicitly requested.
        }

        virtual ~CameraComponent() = default;

        // --- Projection Settings ---
        // Sets up a perspective projection matrix.
        // - fovRadians: Field of View in radians (vertical FOV).
        // - aspectRatio: Width / Height of the viewport.
        // - nearPlane: Distance to the near clipping plane.
        // - farPlane: Distance to the far clipping plane.
        void SetPerspective(float fovRadians, float aspectRatio, float nearPlane, float farPlane) {
            m_FovRadians = fovRadians;
            m_AspectRatio = aspectRatio;
            m_NearPlane = nearPlane;
            m_FarPlane = farPlane;
            m_IsOrthographic = false;
            RecalculateProjectionMatrix();
        }

        // Sets up an orthographic projection matrix.
        // Defines a 2D box in view space that maps to the viewport.
        void SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
            m_OrthoLeft = left;
            m_OrthoRight = right;
            m_OrthoBottom = bottom;
            m_OrthoTop = top;
            m_NearPlane = nearPlane;
            m_FarPlane = farPlane;
            m_IsOrthographic = true;
            RecalculateProjectionMatrix();
        }

        // Adjusts the orthographic size (e.g., for a 2D camera zoom).
        // `size` could be half-height, and aspect ratio is used to determine width.
        void SetOrthographicSize(float size, float aspectRatio) {
            if (m_IsOrthographic) {
                m_OrthoSize = size;
                m_AspectRatio = aspectRatio; // Update aspect ratio as well
                float halfHeight = m_OrthoSize;
                float halfWidth = m_OrthoSize * m_AspectRatio;
                SetOrthographic(-halfWidth, halfWidth, -halfHeight, halfHeight, m_NearPlane, m_FarPlane);
            }
        }


        // --- View Matrix Calculation ---
        // Updates the view matrix based on the provided TransformComponent's state.
        // This is typically called by the Scene before rendering or by the camera controller.
        void UpdateViewMatrix(const TransformComponent& transform) {
            const glm::vec3& position = transform.GetPosition();
            // Use GetForward() from TransformComponent, which is calculated from its rotation.
            // The target for lookAt is position + forward.
            glm::vec3 target = position + transform.GetForward();
            glm::vec3 up = transform.GetUp(); // GetUp is also calculated from rotation

            m_ViewMatrix = glm::lookAt(position, target, up);
        }


        // --- Accessors ---
        const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
        const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }

        float GetNearPlane() const { return m_NearPlane; }
        float GetFarPlane() const { return m_FarPlane; }
        float GetFov() const { return m_IsOrthographic ? 0.0f : m_FovRadians; } // FOV only for perspective
        float GetAspectRatio() const { return m_AspectRatio; }
        bool IsOrthographic() const { return m_IsOrthographic; }

        // --- Component Lifecycle (Optional) ---
        // void OnAttach() override;
        // void Update(float deltaTime) override; // e.g., for camera shake, smooth follow, etc.

    private:
        void RecalculateProjectionMatrix() {
            if (m_IsOrthographic) {
                m_ProjectionMatrix = glm::ortho(m_OrthoLeft, m_OrthoRight, m_OrthoBottom, m_OrthoTop, m_NearPlane, m_FarPlane);
            } else {
                m_ProjectionMatrix = glm::perspective(m_FovRadians, m_AspectRatio, m_NearPlane, m_FarPlane);
            }
            // Apply Vulkan NDC correction (Y-flip and potentially depth range adjustment if not using 0-1)
            // GLM's perspective and ortho are designed for OpenGL's NDC (-1 to 1 depth, Y up).
            // Vulkan NDC: Y down, Depth 0 to 1.
            // The Y-flip is common:
            m_ProjectionMatrix[1][1] *= -1;

            // If depth range was -1 to 1 and Vulkan needs 0 to 1:
            // (This is often handled by viewport settings or depth clear values,
            // but some prefer to bake it into the projection matrix).
            // For GLM_FORCE_DEPTH_ZERO_TO_ONE defined, glm::perspective already does this.
            // If not, manual adjustment might look like:
            // glm::mat4 vulkanClip = glm::mat4(1.0f,  0.0f, 0.0f, 0.0f,
            //                                0.0f, -1.0f, 0.0f, 0.0f, // Y-flip
            //                                0.0f,  0.0f, 0.5f, 0.0f, // Depth remap
            //                                0.0f,  0.0f, 0.5f, 1.0f);
            // m_ProjectionMatrix = vulkanClip * m_ProjectionMatrix;
            // For simplicity, we only apply the Y-flip, assuming GLM_FORCE_DEPTH_ZERO_TO_ONE
            // is defined or Vulkan's depth handling (clear to 1.0, compare op LESS) works correctly.
        }

        // View Matrix (World to Camera Space)
        glm::mat4 m_ViewMatrix = glm::mat4(1.0f); // Initialized to identity

        // Projection Matrix (Camera Space to Clip Space)
        glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);

        // Projection Parameters
        bool m_IsOrthographic = false;
        float m_FovRadians = glm::radians(45.0f);
        float m_AspectRatio = 16.0f / 9.0f;
        float m_NearPlane = 0.1f;
        float m_FarPlane = 1000.0f;

        // Orthographic specific parameters
        float m_OrthoLeft = -1.0f;
        float m_OrthoRight = 1.0f;
        float m_OrthoBottom = -1.0f;
        float m_OrthoTop = 1.0f;
        float m_OrthoSize = 5.0f; // For SetOrthographicSize, e.g., half-height
    };

} // namespace VulkEng

#pragma once

#include "scene/Component.h" // Base class for components
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // For translate, rotate, scale
#include <glm/gtc/quaternion.hpp>       // For glm::quat and quaternion operations
#include <glm/gtx/quaternion.hpp>       // For glm::toMat4 and other helpers
#include <glm/gtx/matrix_decompose.hpp> // For decomposing matrix (optional)

namespace VulkEng {

    class TransformComponent : public Component {
    public:
        TransformComponent() {
            RecalculateMatrix(); // Initialize matrix on creation
        }
        virtual ~TransformComponent() = default;

        // --- Position ---
        const glm::vec3& GetPosition() const { return m_Position; }
        void SetPosition(const glm::vec3& position) {
            m_Position = position;
            m_IsDirty = true;
        }
        void Translate(const glm::vec3& delta) {
            m_Position += delta;
            m_IsDirty = true;
        }

        // --- Rotation (using Quaternions) ---
        const glm::quat& GetRotation() const { return m_Rotation; }
        void SetRotation(const glm::quat& rotation) {
            m_Rotation = glm::normalize(rotation); // Always store normalized quaternions
            m_IsDirty = true;
        }
        // Sets rotation using Euler angles (in radians: yaw, pitch, roll / YXZ order common)
        void SetEulerAngles(const glm::vec3& eulerAnglesRadians) {
            // Common Euler order: Y (yaw), then X (pitch), then Z (roll)
            // glm::quat qPitch = glm::angleAxis(eulerAnglesRadians.x, glm::vec3(1,0,0));
            // glm::quat qYaw   = glm::angleAxis(eulerAnglesRadians.y, glm::vec3(0,1,0));
            // glm::quat qRoll  = glm::angleAxis(eulerAnglesRadians.z, glm::vec3(0,0,1));
            // m_Rotation = qYaw * qPitch * qRoll; // Order matters!
            m_Rotation = glm::normalize(glm::quat(eulerAnglesRadians)); // GLM default is YXZ intrinsic
            m_IsDirty = true;
        }
        // Gets rotation as Euler angles (in radians)
        glm::vec3 GetEulerAngles() const {
            return glm::eulerAngles(m_Rotation); // GLM default is YXZ intrinsic
        }
        // Rotates by a delta quaternion (world space or local space needs to be defined)
        // This applies delta in local space of current rotation: newRotation = currentRotation * deltaRotation
        void Rotate(const glm::quat& deltaRotation) {
            m_Rotation = glm::normalize(m_Rotation * glm::normalize(deltaRotation));
            m_IsDirty = true;
        }
        // Rotates around an axis by an angle (radians)
        void RotateAroundAxis(const glm::vec3& axis, float angleRadians) {
            glm::quat rotDelta = glm::angleAxis(angleRadians, glm::normalize(axis));
            m_Rotation = glm::normalize(m_Rotation * rotDelta);
            m_IsDirty = true;
        }


        // --- Scale ---
        const glm::vec3& GetScale() const { return m_Scale; }
        void SetScale(const glm::vec3& scale) {
            m_Scale = scale;
            m_IsDirty = true;
        }
        void SetScale(float uniformScale) {
            m_Scale = glm::vec3(uniformScale);
            m_IsDirty = true;
        }


        // --- Matrix Operations ---
        // Gets the local transformation matrix (relative to parent, if any)
        // For now, since no hierarchy, this is also the world matrix.
        const glm::mat4& GetLocalMatrix() const {
            if (m_IsDirty) {
                RecalculateMatrix();
            }
            return m_LocalToWorldMatrix; // Renamed for clarity, currently acts as world
        }

        // Gets the world transformation matrix.
        // If hierarchy is implemented, this would combine parent's world matrix with local matrix.
        const glm::mat4& GetWorldMatrix() const {
            // TODO: If hierarchy exists, this needs to be:
            // if (m_ParentTransform && m_ParentTransform->IsDirty()) m_IsDirty = true; // If parent moved, we are dirty
            if (m_IsDirty) {
                RecalculateMatrix();
                // if (m_ParentTransform) {
                //     m_WorldMatrix = m_ParentTransform->GetWorldMatrix() * m_LocalMatrix;
                // } else {
                //     m_WorldMatrix = m_LocalMatrix;
                // }
            }
            return m_LocalToWorldMatrix; // For now, local IS world
        }

        // --- Directional Vectors (Calculated from rotation, in World Space) ---
        // Assumes standard coordinate system: +X right, +Y up, +Z backward (or forward, be consistent)
        // Or if using a "look-at" system, these are derived from that.
        // Let's assume default GLM object space: +Z is "into screen" (forward if looking at -Z)
        glm::vec3 GetForward() const {
            // Forward is often considered -Z in object space if +Z is "out of screen"
            // Or +Z if +Z is "into screen". Let's use -Z local as forward for lookAt.
            return glm::normalize(m_Rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        }
        glm::vec3 GetRight() const {
            return glm::normalize(m_Rotation * glm::vec3(1.0f, 0.0f, 0.0f));
        }
        glm::vec3 GetUp() const {
            return glm::normalize(m_Rotation * glm::vec3(0.0f, 1.0f, 0.0f));
        }

        // --- LookAt Functionality ---
        // Orients the transform to look at a target position from its current position.
        // worldUp defines the "up" direction in world space (usually {0,1,0} or {0,0,1}).
        void LookAt(const glm::vec3& targetPosition, const glm::vec3& worldUp = glm::vec3(0.0f, 1.0f, 0.0f)) {
            if (glm::length(targetPosition - m_Position) < glm::epsilon<float>()) {
                // Already at target or too close, do nothing to avoid issues with lookAt
                return;
            }
            // glm::lookAt creates a VIEW matrix. We need the inverse of the rotation part.
            // The rotation part of a view matrix is the inverse of the world rotation.
            // So, the world rotation is the inverse of the view matrix's rotation part.
            // For an orthonormal matrix (like a rotation matrix), inverse is transpose.
            glm::mat4 lookAtMatrix = glm::lookAt(m_Position, targetPosition, worldUp);
            m_Rotation = glm::normalize(glm::conjugate(glm::quat_cast(lookAtMatrix))); // Conjugate for inverse view rotation
            m_IsDirty = true;
        }


        // --- Component Lifecycle ---
        // void OnAttach() override; // Example if needed
        // void Update(float deltaTime) override; // If transform needs per-frame logic (e.g., animations)

    private:
        // Recalculates the local transformation matrix from position, rotation, and scale.
        void RecalculateMatrix() const { // Made const to allow call from const Get...Matrix()
            glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), m_Position);
            glm::mat4 rotationMat = glm::toMat4(m_Rotation); // Convert quaternion to rotation matrix
            glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), m_Scale);

            // TRS order: Scale, then Rotate, then Translate
            m_LocalToWorldMatrix = translationMat * rotationMat * scaleMat;
            m_IsDirty = false; // Matrix is now up-to-date
        }

        glm::vec3 m_Position = glm::vec3(0.0f);
        glm::quat m_Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion (w, x, y, z)
        glm::vec3 m_Scale    = glm::vec3(1.0f);

        // Cached local-to-world matrix (or local-to-parent if hierarchy exists)
        // Mutable to allow recalculation in const getter methods (dirty flag pattern)
        mutable glm::mat4 m_LocalToWorldMatrix = glm::mat4(1.0f);
        mutable bool m_IsDirty = true; // Flag to indicate if matrix needs recalculation

        // TODO: For scene hierarchy:
        // TransformComponent* m_Parent = nullptr;
        // std::vector<TransformComponent*> m_Children;
    };

} // namespace VulkEng

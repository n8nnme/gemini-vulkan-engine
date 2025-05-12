#include "MeshComponent.h"
#include "scene/GameObject.h" // Optional: For logging or advanced interaction with GameObject
#include "core/Log.h"         // Optional: For logging specific MeshComponent events

namespace VulkEng {

    // --- Constructor (if not defaulted or inlined in header) ---
    // MeshComponent::MeshComponent() {
    //     // VKENG_TRACE("MeshComponent Created for GameObject (if available): {}",
    //     //             m_GameObject ? m_GameObject->GetName() : "UNATTACHED");
    // }

    // --- Destructor (if not defaulted or inlined in header) ---
    // MeshComponent::~MeshComponent() {
    //     // m_Meshes vector of raw pointers will just be cleared.
    //     // No ownership of Mesh objects themselves.
    //     // VKENG_TRACE("MeshComponent Destroyed for GameObject (if available): {}",
    //     //             m_GameObject ? m_GameObject->GetName() : "DETACHED");
    // }


    // --- Method Implementations (if moved from header) ---

    // AddMesh is currently inline in MeshComponent.h
    /*
    void MeshComponent::AddMesh(const Mesh* mesh) {
        if (mesh) {
            m_Meshes.push_back(mesh);
            // VKENG_TRACE("MeshComponent: Added mesh (Vertices: {}, Indices: {}) to GameObject '{}'",
            //             mesh->vertexCount, mesh->indexCount,
            //             m_GameObject ? m_GameObject->GetName() : "UNATTACHED");
        } else {
            VKENG_WARN("MeshComponent: Attempted to add a null mesh pointer to GameObject '{}'.",
                       m_GameObject ? m_GameObject->GetName() : "UNATTACHED");
        }
    }
    */

    // AddMeshes is currently inline in MeshComponent.h
    /*
    void MeshComponent::AddMeshes(const std::vector<const Mesh*>& meshes) {
        for (const Mesh* mesh : meshes) {
            AddMesh(mesh); // Reuse single AddMesh logic and its logging/checks
        }
    }
     void MeshComponent::AddMeshes(const std::vector<Mesh>& meshes) {
         for (const auto& mesh : meshes) {
             AddMesh(&mesh);
         }
     }
    */


    // GetMeshes is currently inline in MeshComponent.h
    /*
    const std::vector<const Mesh*>& MeshComponent::GetMeshes() const {
        return m_Meshes;
    }
    */

    // ClearMeshes is currently inline in MeshComponent.h
    /*
    void MeshComponent::ClearMeshes() {
        // VKENG_TRACE("MeshComponent: Clearing {} meshes from GameObject '{}'.",
        //            m_Meshes.size(), m_GameObject ? m_GameObject->GetName() : "UNATTACHED");
        m_Meshes.clear();
    }
    */


    // --- Optional: Component Lifecycle Methods Implementation ---
    // void MeshComponent::OnAttach() {
    //     Component::OnAttach(); // Call base implementation if it exists
    //     VKENG_TRACE("MeshComponent OnAttach for GameObject '{}'",
    //                 m_GameObject ? m_GameObject->GetName() : "UNATTACHED");
    //     // Example: If this component needed to find other components on attach:
    //     // if (m_GameObject) {
    //     //     auto* transform = m_GameObject->GetComponent<TransformComponent>();
    //     //     if (!transform) {
    //     //         VKENG_WARN("MeshComponent on '{}' attached, but no TransformComponent found!", m_GameObject->GetName());
    //     //     }
    //     // }
    // }

    // void MeshComponent::OnDetach() {
    //     Component::OnDetach(); // Call base implementation
    //     VKENG_TRACE("MeshComponent OnDetach for GameObject '{}'",
    //                 m_GameObject ? m_GameObject->GetName() : "UNATTACHED");
    //     ClearMeshes(); // Good practice to clear internal references on detach
    // }

    // Update is unlikely to be needed for a simple mesh container.
    // void MeshComponent::Update(float deltaTime) {
    //     // Component::Update(deltaTime); // Call base
    //     // Logic for animating meshes directly, LOD selection, etc., could go here
    //     // if not handled by a more specialized system or component.
    // }


    // This .cpp file primarily serves as a placeholder for non-inline implementations
    // if the methods in MeshComponent.h become more complex.

} // namespace VulkEng

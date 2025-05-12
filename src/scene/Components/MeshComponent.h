#pragma once

#include "scene/Component.h" // Base class for components
#include "assets/Mesh.h"     // Definition of the Mesh struct (which holds GPU buffer handles)
#include "assets/Material.h" // For MaterialHandle (though Mesh struct already contains it)

#include <vector>
#include <memory> // Not strictly needed here if Mesh struct doesn't use it directly

namespace VulkEng {

    // Forward declaration (Mesh is already included, but good practice if it wasn't)
    // struct Mesh;

    // A component that holds references to one or more Mesh objects for rendering.
    // It allows a GameObject to be visually represented in the scene.
    class MeshComponent : public Component {
    public:
        MeshComponent() = default;
        virtual ~MeshComponent() = default;

        // Adds a pointer to a Mesh object to this component's list of meshes.
        // The Mesh objects themselves (and their GPU buffers) are typically managed
        // by the AssetManager. This component only stores pointers.
        // `mesh` should be a valid pointer to a Mesh loaded and managed elsewhere.
        void AddMesh(const Mesh* mesh) {
            if (mesh) {
                m_Meshes.push_back(mesh);
            }
            // else { VKENG_WARN("MeshComponent: Attempted to add a null mesh pointer."); }
        }

        // Adds multiple meshes at once.
        void AddMeshes(const std::vector<const Mesh*>& meshes) {
            for (const Mesh* mesh : meshes) {
                AddMesh(mesh); // Reuse single AddMesh logic
            }
        }
         void AddMeshes(const std::vector<Mesh>& meshes) { // Convenience for vector of Mesh objects
             for (const auto& mesh : meshes) {
                 AddMesh(&mesh);
             }
         }


        // Retrieves the list of mesh pointers held by this component.
        // Used by the rendering system to know what to draw for this GameObject.
        const std::vector<const Mesh*>& GetMeshes() const {
            return m_Meshes;
        }

        // Clears all meshes from this component.
        void ClearMeshes() {
            m_Meshes.clear();
        }

        // Optional: If meshes could have individual visibility or overrides.
        // void SetMeshVisibility(size_t index, bool visible);
        // bool IsMeshVisible(size_t index) const;

        // --- Component Lifecycle Methods (Optional Overrides) ---
        // void OnAttach() override;
        // void OnDetach() override;
        // void Update(float deltaTime) override; // Unlikely to be needed for a simple mesh container

    private:
        // A list of non-owning pointers to Mesh objects.
        // The actual Mesh objects and their GPU resources are owned by AssetManager.
        std::vector<const Mesh*> m_Meshes;

        // Optional: Per-mesh instance data if this component represents multiple instances
        // of the same set of meshes with different properties (e.g., different materials or transforms
        // if not handled by separate GameObjects). For this engine, each GameObject with a
        // MeshComponent will likely have its own unique transform via TransformComponent.
    };

} // namespace VulkEng

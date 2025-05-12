#pragma once

#include "Mesh.h"     // For MeshData (CPU-side vertex/index data per mesh part)
#include <string>
#include <vector>
#include <memory>      // Not strictly needed here, but often associated with asset management
#include <glm/glm.hpp> // For glm::vec3 (used in allVerticesPhysics)

// Forward declare Assimp types to avoid including heavy Assimp headers in this public header.
// The implementation in ModelLoader.cpp will include the actual Assimp headers.
struct aiScene;
struct aiNode;
struct aiMesh;
struct aiMaterial;

namespace VulkEng {

    // Forward declaration (VulkanContext is not directly used by ModelLoader's public interface anymore,
    // as it focuses on extracting CPU data. AssetManager handles GPU upload).
    // class VulkanContext;

    // Structure to hold data extracted from an Assimp material.
    // This is CPU-side information, like texture paths and material properties.
    struct MaterialDataSource { // Renamed to avoid conflict with engine's Material struct
        std::string name;                    // Name of the material from the model file
        std::string diffuseTexturePath;      // File path to the diffuse (base color) texture
        // std::string normalTexturePath;
        // std::string metallicRoughnessTexturePath;
        // std::string emissiveTexturePath;
        // std::string aoTexturePath;

        glm::vec4 baseColorFactor = glm::vec4(1.0f); // Default if no texture or for modulation
        // float metallicFactor = 1.0f;
        // float roughnessFactor = 1.0f;
        // glm::vec3 emissiveFactor = glm::vec3(0.0f);
        // Add other PBR properties as needed
    };

    // Structure to hold all CPU-side data extracted from a loaded model file.
    // This includes mesh data for rendering, material data, and combined geometry for physics.
    struct LoadedModelData {
        std::string filePath; // Store the original file path for reference

        // Per-submesh data primarily for rendering.
        // Each MeshData contains vertices, indices, and an index to `materialsFromFile`.
        std::vector<MeshData> meshesForRender;

        // Material information extracted directly from the model file.
        // AssetManager will use this to create engine Material instances.
        std::vector<MaterialDataSource> materialsFromFile;

        // Combined geometry from all submeshes, primarily for creating physics collision shapes
        // (e.g., a single btBvhTriangleMeshShape for the entire static model).
        std::vector<glm::vec3> allVerticesPhysics; // Only positions
        std::vector<uint32_t> allIndicesPhysics;   // Indices for the combined vertex list
    };

    // Static class responsible for loading 3D model files using Assimp
    // and extracting CPU-side data into the LoadedModelData structure.
    // It does not handle GPU resource creation (that's AssetManager's job).
    class ModelLoader {
    public:
        ModelLoader() = delete; // Static class, no instances

        // Loads a model from the given file path and populates `outModelData`.
        // Returns true on success, false on failure.
        static bool LoadModel(const std::string& filepath, LoadedModelData& outModelData);

    private:
        // Recursively processes an Assimp node and its children.
        static void ProcessAssimpNode(
            aiNode* node,
            const aiScene* scene,
            LoadedModelData& outModelData,
            const std::string& modelDirectory // Directory of the model file for resolving relative texture paths
        );

        // Processes an individual Assimp mesh (aiMesh) and converts it to engine's MeshData.
        static MeshData ProcessAssimpMesh(
            aiMesh* mesh,
            const aiScene* scene, // For accessing materials if needed (though materialIndex is stored)
            const std::string& modelDirectory
        );

        // Processes an Assimp material (aiMaterial) and extracts relevant information.
        static MaterialDataSource ProcessAssimpMaterial(
            aiMaterial* material,
            const aiScene* scene, // For embedded textures
            const std::string& modelDirectory
        );

        // Helper to load texture paths from an Assimp material for a specific type.
        // static std::vector<std::string> LoadMaterialTexturePaths(
        //     aiMaterial* mat,
        //     aiTextureType type,
        //     const std::string& modelDirectory
        // );
    };

} // namespace VulkEng



// 12 May 20:05 | yeah youh
// #pragma once
// 
// #include "Mesh.h"     // For MeshData
// #include <string>
// #include <vector>
// #include <memory>
// #include <glm/glm.hpp> // For glm::vec3 in allVertices
// 
// // Forward declare Assimp types
// struct aiScene;
// struct aiNode;
// struct aiMesh;
// struct aiMaterial;
// 
// namespace VulkEng {
//     class VulkanContext; // Forward declaration
// 
//     // Data extracted from Assimp material (texture paths, properties)
//     struct MaterialData {
//          std::string name;
//          std::string diffuseTexturePath;
//          glm::vec4 baseColorFactor = glm::vec4(1.0f);
//          // Add paths/factors for normal, metallic, roughness, emissive etc.
//     };
// 
//     // Structure to hold all data loaded from a model file
//     struct LoadedModelData {
//         std::vector<MeshData> meshesForRender; // Data for rendering meshes
//         std::vector<MaterialData> materials;   // Extracted material info
// 
//         // Combined geometry for physics shape generation (TriangleMesh)
//         std::vector<glm::vec3> allVerticesPhysics;
//         std::vector<uint32_t> allIndicesPhysics;
//     };
// 
//     class ModelLoader {
//     public:
//         // VulkanContext might not be needed here if ModelLoader only extracts CPU data
//         static bool LoadModel(const std::string& filepath, LoadedModelData& outModelData);
// 
//     private:
//         static void ProcessNode(aiNode* node, const aiScene* scene, LoadedModelData& outModelData, const std::string& directory);
//         static MeshData ProcessMesh(aiMesh* mesh, const aiScene* scene, const std::string& directory);
//         static MaterialData ProcessMaterial(aiMaterial* material, const aiScene* scene, const std::string& directory);
//     };
// }

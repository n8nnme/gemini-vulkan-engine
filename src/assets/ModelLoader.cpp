#include "ModelLoader.h"
#include "core/Log.h"       // For logging loading progress and errors

#include <assimp/Importer.hpp>      // Assimp's C++ Importer interface
#include <assimp/scene.h>           // For aiScene, aiNode, aiMesh, aiMaterial
#include <assimp/postprocess.h>   // For post-processing flags
#include <assimp/material.h>      // For aiMaterialKeys and aiGetMaterialTexture

#include <filesystem> // For robust path handling (C++17)
#include <algorithm>  // For std::replace

namespace VulkEng {

    bool ModelLoader::LoadModel(const std::string& filepath, LoadedModelData& outModelData) {
        VKENG_INFO("ModelLoader: Attempting to load model from '{}'", filepath);

        // Ensure the file exists before attempting to load
        if (!std::filesystem::exists(filepath)) {
            VKENG_ERROR("ModelLoader: File not found at path '{}'", filepath);
            return false;
        }
        outModelData.filePath = filepath; // Store the original path

        Assimp::Importer importer;

        // Common post-processing flags for game assets:
        // - aiProcess_Triangulate: Converts all primitive faces to triangles.
        // - aiProcess_GenSmoothNormals: Generates smooth normals if they are missing (good for lighting).
        //   (Use aiProcess_GenNormals for flat normals if preferred).
        // - aiProcess_FlipUVs: Flips texture coordinates vertically (often needed for OpenGL/Vulkan conventions).
        // - aiProcess_CalcTangentSpace: Calculates tangents and bitangents (needed for normal mapping).
        // - aiProcess_JoinIdenticalVertices: Optimizes mesh by merging identical vertices.
        // - aiProcess_SortByPType: Removes non-triangle/line/point primitives if not handled by other flags.
        // - aiProcess_ValidateDataStructure: Validates the imported data.
        // - aiProcess_OptimizeMeshes: Tries to reduce number of meshes.
        // - aiProcess_OptimizeGraph: Tries to optimize node hierarchy.
        // - aiProcess_ImproveCacheLocality: Reorders triangles for better cache performance.
        const unsigned int ppsteps = aiProcess_Triangulate |
                                     aiProcess_GenSmoothNormals |
                                     aiProcess_FlipUVs |
                                     aiProcess_CalcTangentSpace |
                                     aiProcess_JoinIdenticalVertices |
                                     aiProcess_SortByPType |
                                     aiProcess_ValidateDataStructure |
                                     aiProcess_OptimizeMeshes |
                                     aiProcess_ImproveCacheLocality;

        const aiScene* scene = importer.ReadFile(filepath, ppsteps);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            VKENG_ERROR("ModelLoader: Assimp error while loading model '{}': {}", filepath, importer.GetErrorString());
            return false;
        }

        // Get the directory of the model file for resolving relative texture paths
        std::string modelDirectory = std::filesystem::path(filepath).parent_path().string();
        if (modelDirectory.empty() || modelDirectory == ".") { // If file is in current dir or path had no slash
             modelDirectory = "."; // Use current directory
        }
        // Ensure consistent slashes for directory path
        std::replace(modelDirectory.begin(), modelDirectory.end(), '\\', '/');
        if (!modelDirectory.empty() && modelDirectory.back() != '/') {
            modelDirectory += '/';
        }


        // Clear any previous data in outModelData
        outModelData.meshesForRender.clear();
        outModelData.materialsFromFile.clear();
        outModelData.allVerticesPhysics.clear();
        outModelData.allIndicesPhysics.clear();

        // 1. Process all materials defined in the scene
        if (scene->HasMaterials()) {
            outModelData.materialsFromFile.reserve(scene->mNumMaterials);
            VKENG_INFO("ModelLoader: Processing {} materials from file...", scene->mNumMaterials);
            for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
                aiMaterial* assimpMaterial = scene->mMaterials[i];
                outModelData.materialsFromFile.push_back(ProcessAssimpMaterial(assimpMaterial, scene, modelDirectory));
            }
        } else {
            VKENG_INFO("ModelLoader: Model has no embedded materials.");
        }

        // 2. Recursively process the scene graph starting from the root node
        VKENG_INFO("ModelLoader: Processing scene graph nodes...");
        ProcessAssimpNode(scene->mRootNode, scene, outModelData, modelDirectory);

        VKENG_INFO("ModelLoader: Successfully loaded and processed model '{}'.", filepath);
        VKENG_INFO("  Render Meshes: {}, Materials: {}, Physics Verts: {}, Physics Idx: {}",
                   outModelData.meshesForRender.size(), outModelData.materialsFromFile.size(),
                   outModelData.allVerticesPhysics.size(), outModelData.allIndicesPhysics.size());

        return true;
    }

    void ModelLoader::ProcessAssimpNode(aiNode* node, const aiScene* scene, LoadedModelData& outModelData, const std::string& modelDirectory) {
        // Process all meshes attached to the current node
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            // scene->mMeshes contains all meshes in the scene.
            // node->mMeshes[i] is an index into scene->mMeshes.
            aiMesh* assimpMesh = scene->mMeshes[node->mMeshes[i]];
            MeshData engineMeshData = ProcessAssimpMesh(assimpMesh, scene, modelDirectory);

            // Aggregate vertices and indices for the physics shape
            // Ensure that indices are adjusted based on the current size of allVerticesPhysics
            uint32_t currentVertexOffset = static_cast<uint32_t>(outModelData.allVerticesPhysics.size());
            for (const auto& vert : engineMeshData.vertices) { // Iterate over vertices in THIS MeshData
                outModelData.allVerticesPhysics.push_back(vert.position); // Only store position
            }
            for (uint32_t index : engineMeshData.indices) { // Iterate over indices in THIS MeshData
                outModelData.allIndicesPhysics.push_back(currentVertexOffset + index);
            }

            outModelData.meshesForRender.push_back(std::move(engineMeshData)); // Add processed mesh to render list
        }

        // Recursively process all child nodes
        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            ProcessAssimpNode(node->mChildren[i], scene, outModelData, modelDirectory);
        }
    }

    MeshData ModelLoader::ProcessAssimpMesh(aiMesh* mesh, const aiScene* scene, const std::string& modelDirectory) {
        MeshData data;
        data.name = mesh->mName.C_Str();
        data.vertices.reserve(mesh->mNumVertices);

        // Process vertices
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            Vertex vertex;

            // Position
            vertex.position.x = mesh->mVertices[i].x;
            vertex.position.y = mesh->mVertices[i].y;
            vertex.position.z = mesh->mVertices[i].z;

            // Normals (if they exist)
            if (mesh->HasNormals()) {
                vertex.normal.x = mesh->mNormals[i].x;
                vertex.normal.y = mesh->mNormals[i].y;
                vertex.normal.z = mesh->mNormals[i].z;
            } else {
                vertex.normal = glm::vec3(0.0f, 0.0f, 0.0f); // Default if no normals
            }

            // Texture Coordinates (Assimp can have multiple sets; use the first one, channel 0)
            if (mesh->HasTextureCoords(0)) { // mTextureCoords[0]
                vertex.texCoord.x = mesh->mTextureCoords[0][i].x;
                vertex.texCoord.y = mesh->mTextureCoords[0][i].y;
                // Assimp's (u,v,w) - we usually only need (u,v)
            } else {
                vertex.texCoord = glm::vec2(0.0f, 0.0f); // Default if no UVs
            }

            // Vertex Colors (Assimp can have multiple sets; use the first one)
            if (mesh->HasVertexColors(0)) { // mColors[0]
                vertex.color.r = mesh->mColors[0][i].r;
                vertex.color.g = mesh->mColors[0][i].g;
                vertex.color.b = mesh->mColors[0][i].b;
                vertex.color.a = mesh->mColors[0][i].a;
            } else {
                vertex.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // Default white
            }

            // Tangents (if they exist, calculated by aiProcess_CalcTangentSpace)
            if (mesh->HasTangentsAndBitangents()) {
                vertex.tangent.x = mesh->mTangents[i].x;
                vertex.tangent.y = mesh->mTangents[i].y;
                vertex.tangent.z = mesh->mTangents[i].z;
                // We can calculate bitangent in shader: cross(normal, tangent) * handedness
                // Or load mesh->mBitangents[i] if needed directly.
            } else {
                vertex.tangent = glm::vec3(0.0f, 0.0f, 0.0f);
            }
            data.vertices.push_back(vertex);
        }

        // Process indices
        data.indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3); // Assuming triangles
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            aiFace face = mesh->mFaces[i];
            if (face.mNumIndices != 3) { // Should be triangles due to aiProcess_Triangulate
                VKENG_WARN("ModelLoader: Mesh '{}' has a face with {} indices (expected 3). Skipping face.", data.name, face.mNumIndices);
                continue;
            }
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                data.indices.push_back(face.mIndices[j]);
            }
        }

        // Store the material index associated with this mesh (from Assimp's material array)
        data.materialIndex = mesh->mMaterialIndex;

        return data;
    }

    MaterialDataSource ModelLoader::ProcessAssimpMaterial(aiMaterial* material, const aiScene* scene, const std::string& modelDirectory) {
        MaterialDataSource matData;

        // Get material name
        aiString name;
        material->Get(AI_MATKEY_NAME, name);
        matData.name = (name.length > 0) ? name.C_Str() : "UnnamedMaterial_" + std::to_string(reinterpret_cast<uintptr_t>(material));

        // Get base color factor (diffuse color if not PBR)
        aiColor4D diffuseColor;
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == AI_SUCCESS) {
            matData.baseColorFactor = {diffuseColor.r, diffuseColor.g, diffuseColor.b, diffuseColor.a};
        } else {
            matData.baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f}; // Default white
        }
        // For PBR, AI_MATKEY_BASE_COLOR might be more appropriate if available from format (e.g. glTF)
        // aiColor4D baseColorPBR;
        // if (material->Get(AI_MATKEY_BASE_COLOR, baseColorPBR) == AI_SUCCESS) {
        //     matData.baseColorFactor = {baseColorPBR.r, baseColorPBR.g, baseColorPBR.b, baseColorPBR.a};
        // }


        // --- Diffuse (Base Color) Texture ---
        // Assimp uses aiTextureType_DIFFUSE for base color in many PBR workflows too
        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString aiPath;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath) == AI_SUCCESS) {
                std::string texturePathStr = aiPath.C_Str();
                if (texturePathStr.rfind('*', 0) == 0) { // Embedded texture (e.g., *0, *1)
                    int textureIndex = std::stoi(texturePathStr.substr(1));
                    if (scene->HasTextures() && textureIndex < static_cast<int>(scene->mNumTextures)) {
                        aiTexture* embeddedTexture = scene->mTextures[textureIndex];
                        // mHeight == 0 means compressed (e.g. jpg, png embedded as bytes)
                        // mHeight > 0 means raw ARGB data
                        if (embeddedTexture->mHeight == 0) {
                             VKENG_WARN("ModelLoader: Material '{}' has embedded compressed texture '{}'. "
                                       "Loading from embedded raw bytes not implemented yet. Path: '{}'",
                                       matData.name, embeddedTexture->mFilename.C_Str(), texturePathStr);
                            // AssetManager would need to load this from memory (stbi_load_from_memory)
                            // matData.diffuseTexturePath = "*EMBEDDED*" + texturePathStr; // Mark it
                        } else {
                             VKENG_WARN("ModelLoader: Material '{}' has embedded raw ARGB texture. Not supported yet.", matData.name);
                        }
                    } else {
                         VKENG_ERROR("ModelLoader: Invalid embedded texture index '{}' for material '{}'.", texturePathStr, matData.name);
                    }
                } else { // External texture file
                    std::filesystem::path texPath(texturePathStr);
                    std::filesystem::path fullPath;
                    if (texPath.is_absolute()) {
                        fullPath = texPath;
                    } else {
                        fullPath = std::filesystem::path(modelDirectory) / texPath;
                    }
                    // Normalize and convert to string
                    matData.diffuseTexturePath = std::filesystem::lexically_normal(fullPath).string();
                    std::replace(matData.diffuseTexturePath.begin(), matData.diffuseTexturePath.end(), '\\', '/');
                }
            }
        }

        // TODO: Load other PBR textures (Normal, MetallicRoughness, Emissive, AO)
        // Normal Map: aiTextureType_NORMALS
        // Metallic/Roughness: Often packed. GLTF uses aiTextureType_UNKNOWN with specific key hints or
        // aiTextureType_METALNESS and aiTextureType_DIFFUSE_ROUGHNESS (or SHININESS for older specular).
        // Ambient Occlusion: aiTextureType_AMBIENT_OCCLUSION or aiTextureType_LIGHTMAP (can vary by format)
        // Emissive: aiTextureType_EMISSIVE

        // Example for PBR scalar factors (if textures are not present)
        // float metallic = 0.0f, roughness = 1.0f;
        // material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
        // material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
        // matData.metallicFactor = metallic;
        // matData.roughnessFactor = roughness;
        // aiColor3D emissive;
        // if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
        //     matData.emissiveFactor = {emissive.r, emissive.g, emissive.b};
        // }

        return matData;
    }

} // namespace VulkEng








// 12 May 20:07 | 1
// #include "ModelLoader.h"
// #include "core/Log.h"
// #include "graphics/VulkanContext.h" // Only if directly creating Vulkan resources (not current design)
// 
// #include <assimp/Importer.hpp>
// #include <assimp/scene.h>
// #include <assimp/postprocess.h>
// #include <assimp/material.h> // For aiMaterial, aiGetMaterialTexture
// #include <algorithm> // For std::replace
// 
// namespace VulkEng {
// 
//     bool ModelLoader::LoadModel(const std::string& filepath, LoadedModelData& outModelData) {
//         VKENG_INFO("ModelLoader: Loading model: {}", filepath);
//         Assimp::Importer importer;
// 
//         // aiProcess_FlipWindingOrder might be needed if front/back faces are wrong
//         const aiScene* scene = importer.ReadFile(filepath,
//                                                  aiProcess_Triangulate            |
//                                                  aiProcess_GenSmoothNormals       |
//                                                  aiProcess_FlipUVs                | // Common for Vulkan/OpenGL
//                                                  aiProcess_CalcTangentSpace       |
//                                                  aiProcess_JoinIdenticalVertices  |
//                                                  aiProcess_SortByPType            | // Removes points/lines
//                                                  aiProcess_ValidateDataStructure);
// 
//         if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
//             VKENG_ERROR("Assimp Error loading model '{}': {}", filepath, importer.GetErrorString());
//             return false;
//         }
// 
//         std::string directory = filepath.substr(0, filepath.find_last_of("/\\"));
//         if (directory == filepath) directory = "."; // File in current dir
// 
//         // Clear previous data
//         outModelData.meshesForRender.clear();
//         outModelData.materials.clear();
//         outModelData.allVerticesPhysics.clear();
//         outModelData.allIndicesPhysics.clear();
// 
//         // Process materials first
//         if (scene->HasMaterials()) {
//             outModelData.materials.reserve(scene->mNumMaterials);
//             for(unsigned int i = 0; i < scene->mNumMaterials; ++i) {
//                 outModelData.materials.push_back(ProcessMaterial(scene->mMaterials[i], scene, directory));
//             }
//         }
// 
//         // Process nodes and their meshes
//         ProcessNode(scene->mRootNode, scene, outModelData, directory);
// 
//         VKENG_INFO("ModelLoader: Loaded successfully: {} render meshes, {} materials. Physics geometry: {} verts, {} inds.",
//                    outModelData.meshesForRender.size(), outModelData.materials.size(),
//                    outModelData.allVerticesPhysics.size(), outModelData.allIndicesPhysics.size());
//         return true;
//     }
// 
//     void ModelLoader::ProcessNode(aiNode* node, const aiScene* scene, LoadedModelData& outModelData, const std::string& directory) {
//         for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
//             aiMesh* assimpMesh = scene->mMeshes[node->mMeshes[i]];
//             MeshData engineMeshData = ProcessMesh(assimpMesh, scene, directory);
// 
//             // Aggregate vertices and indices for physics shapes
//             uint32_t vertexOffset = static_cast<uint32_t>(outModelData.allVerticesPhysics.size());
//             for(const auto& vert : engineMeshData.vertices) { // Use vertices from MeshData
//                 outModelData.allVerticesPhysics.push_back(vert.pos);
//             }
//             for(uint32_t index : engineMeshData.indices) {
//                 outModelData.allIndicesPhysics.push_back(vertexOffset + index);
//             }
// 
//             outModelData.meshesForRender.push_back(std::move(engineMeshData));
//         }
// 
//         for (unsigned int i = 0; i < node->mNumChildren; ++i) {
//             ProcessNode(node->mChildren[i], scene, outModelData, directory);
//         }
//     }
// 
//     MeshData ModelLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene, const std::string& directory) {
//         MeshData meshData;
//         meshData.vertices.reserve(mesh->mNumVertices);
// 
//         for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
//             Vertex vertex;
//             vertex.pos = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};
//             if (mesh->HasNormals()) vertex.normal = {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};
//             else vertex.normal = {0.0f, 0.0f, 0.0f};
// 
//             if (mesh->HasTextureCoords(0)) vertex.texCoord = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
//             else vertex.texCoord = {0.0f, 0.0f};
// 
//             if (mesh->HasVertexColors(0)) vertex.color = {mesh->mColors[0][i].r, mesh->mColors[0][i].g, mesh->mColors[0][i].b, mesh->mColors[0][i].a};
//             else vertex.color = {1.0f, 1.0f, 1.0f, 1.0f}; // Default white
// 
//             if (mesh->HasTangentsAndBitangents()) vertex.tangent = {mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z};
//             else vertex.tangent = {0.0f, 0.0f, 0.0f};
//             meshData.vertices.push_back(vertex);
//         }
// 
//         meshData.indices.reserve(mesh->mNumFaces * 3);
//         for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
//             aiFace face = mesh->mFaces[i];
//             if (face.mNumIndices != 3) {
//                  VKENG_WARN("ModelLoader: Non-triangular face encountered ({} indices), skipping.", face.mNumIndices);
//                  continue;
//             }
//             for (unsigned int j = 0; j < face.mNumIndices; ++j) {
//                 meshData.indices.push_back(face.mIndices[j]);
//             }
//         }
// 
//         meshData.materialIndex = mesh->mMaterialIndex; // Store Assimp's material index
//         return meshData;
//     }
// 
//     MaterialData ModelLoader::ProcessMaterial(aiMaterial* material, const aiScene* scene, const std::string& directory) {
//         MaterialData matData;
//         aiString name;
//         material->Get(AI_MATKEY_NAME, name);
//         matData.name = name.length > 0 ? name.C_Str() : "UnnamedMaterial_" + std::to_string(reinterpret_cast<uintptr_t>(material));
// 
//         aiColor4D diffuse;
//         if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuse) == AI_SUCCESS) {
//             matData.baseColorFactor = {diffuse.r, diffuse.g, diffuse.b, diffuse.a};
//         }
// 
//         // Diffuse Texture (Base Color Texture in PBR terms)
//         if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
//             aiString path;
//             if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
//                 std::string texturePathStr = path.C_Str();
//                 if (texturePathStr.rfind('*', 0) == 0) { // Embedded texture
//                     VKENG_WARN("ModelLoader: Embedded texture '{}' in material '{}' not supported yet.", texturePathStr, matData.name);
//                 } else {
//                     // Handle relative paths - Assimp paths are often relative to the model file
//                     std::string fullPath = directory + "/" + texturePathStr;
//                     std::replace(fullPath.begin(), fullPath.end(), '\\', '/'); // Normalize
//                     matData.diffuseTexturePath = fullPath;
//                 }
//             }
//         }
//         // TODO: Add support for other PBR textures (Normal, MetallicRoughness, Emissive, AO)
//         // Example for Normal map:
//         // if (material->GetTextureCount(aiTextureType_NORMALS) > 0) { ... }
//         // Example for Metallic/Roughness (often packed, or separate AI_MATKEY_METALLIC_FACTOR, AI_MATKEY_ROUGHNESS_FACTOR):
//         // if (material->GetTextureCount(aiTextureType_UNKNOWN) > 0) { // Check for glTF PBR metallicRoughnessTexture
//         //    ... or aiTextureType_METALNESS and aiTextureType_DIFFUSE_ROUGHNESS if separate ...
//         // }
//         // float metallicFactor, roughnessFactor;
//         // material->Get(AI_MATKEY_METALLIC_FACTOR, metallicFactor);
//         // material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessFactor);
// 
//         return matData;
//     }
// 
// } // namespace VulkEng

#pragma once

#include "Mesh.h"         // For Mesh struct and MeshData
#include "Material.h"     // For Material struct and MaterialHandle
#include "Texture.h"      // For Texture struct and TextureHandle
#include "ModelLoader.h"  // For LoadedModelData struct
#include "graphics/SamplerCache.h" // For managing VkSampler objects

#include <string>
#include <vector>
#include <memory>        // For std::unique_ptr
#include <unordered_map> // For caching assets by path/name
#include <filesystem>    // For path manipulation (C++17)

namespace VulkEng {

    // Forward declarations from other engine parts
    class VulkanContext;
    class CommandManager;
    class VulkanBuffer; // Used internally for GPU buffers

    // Handle types (already defined in their respective headers, but good for context)
    // using ModelHandle = size_t;
    // using MaterialHandle = size_t;
    // using TextureHandle = size_t;
    // const ModelHandle InvalidModelHandle = static_cast<ModelHandle>(-1);
    // const MaterialHandle InvalidMaterialHandle = static_cast<MaterialHandle>(-1);
    // const TextureHandle InvalidTextureHandle = static_cast<TextureHandle>(-1);


    // Manages loading, storage, and retrieval of game assets like models, textures, materials.
    // Handles GPU resource creation for these assets.
    class AssetManager {
    public:
        // Constructor requires VulkanContext for device access and CommandManager for buffer operations.
        AssetManager(VulkanContext& context, CommandManager& commandManager);
        // Destructor handles cleanup of all loaded assets and their GPU resources.
        ~AssetManager();

        // Prevent copying and assignment. AssetManager should be a singleton or unique instance.
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

        // --- Model Loading ---
        // Loads a 3D model from the specified file path.
        // Processes its meshes, materials, and textures.
        // Returns a ModelHandle to reference the loaded model.
        ModelHandle LoadModel(const std::string& filepath);
        // Retrieves the vector of GPU Meshes associated with a loaded model handle.
        const std::vector<Mesh>& GetModelMeshes(ModelHandle handle) const;
        // Retrieves the raw LoadedModelData (CPU-side) for a model, e.g., for physics.
        const LoadedModelData* GetLoadedModelData(ModelHandle handle) const;


        // --- Texture Loading ---
        // Loads a texture from the specified file path.
        // `generateMips`: If true, mipmaps will be generated for the texture.
        // Returns a TextureHandle to reference the loaded texture.
        TextureHandle LoadTexture(const std::string& filepath, bool generateMips = true);
        // Retrieves a reference to a loaded Texture struct by its handle.
        const Texture& GetTexture(TextureHandle handle) const;


        // --- Material Access ---
        // Retrieves a reference to a loaded Material struct by its handle.
        const Material& GetMaterial(MaterialHandle handle) const;
        // Creates a new material descriptor set (if needed by Renderer or other systems)
        // This might be called when a material's texture changes or if descriptor sets are dynamic.
        // For now, descriptor sets are created when materials are loaded with models.
        // VkDescriptorSet CreateMaterialDescriptorSet(MaterialHandle handle); // More advanced use case


        // --- Default Assets ---
        // Provides access to pre-created default assets (e.g., a plain white texture).
        TextureHandle GetDefaultWhiteTexture() const { return m_DefaultWhiteTexture; }
        MaterialHandle GetDefaultMaterial() const { return m_DefaultMaterial; }
        VkSampler GetDefaultSampler() const; // Get default sampler from cache


    private:
        // --- Internal Helper Methods ---
        // Creates engine Material instances from loaded MaterialDataSource.
        // This includes loading referenced textures and allocating/updating descriptor sets.
        MaterialHandle ProcessLoadedMaterial(
            const MaterialDataSource& matDataSource,
            VkDescriptorSetLayout materialSetLayout, // Layout for Set 1 (Material textures)
            VkDescriptorPool descriptorPool         // Pool to allocate from
        );

        // Creates a GPU Mesh object from CPU-side MeshData.
        // This involves creating and populating vertex and index buffers.
        // `materialHandlesForModel` maps Assimp material indices to engine MaterialHandles.
        Mesh CreateGPUMeshFromData(const MeshData& meshData, const std::vector<MaterialHandle>& materialHandlesForModel);

        // Creates a device-local GPU buffer (e.g., for vertices, indices)
        // by first copying data to a host-visible staging buffer.
        std::unique_ptr<VulkanBuffer> CreateDeviceLocalBuffer(
            const void* data,            // Pointer to raw data
            VkDeviceSize bufferSize,     // Total size of the data
            VkBufferUsageFlags usage     // How the buffer will be used (e.g., vertex, index)
        );

        // Initializes default assets like a 1x1 white texture and a default material.
        void CreateDefaultAssets();


        // --- Member Variables ---
        VulkanContext& m_Context;         // Reference to the Vulkan context
        CommandManager& m_CommandManager; // Reference to the command manager for GPU operations
        std::unique_ptr<SamplerCache> m_SamplerCache; // Manages VkSampler objects

        // --- Asset Storage ---
        // Models are stored as a vector of sub-meshes.
        std::vector<std::vector<Mesh>> m_LoadedModels;
        // Raw CPU-side data loaded by ModelLoader, cached for physics, etc.
        std::vector<LoadedModelData> m_CachedModelData;
        // All unique materials created by the engine.
        std::vector<Material> m_LoadedMaterials;
        // All unique textures loaded by the engine.
        std::vector<Texture> m_LoadedTextures;

        // --- Caching Maps (Path/Name to Handle) ---
        // Ensures assets are not loaded multiple times if requested by the same path/name.
        std::unordered_map<std::string, ModelHandle> m_ModelPathToHandleMap;
        std::unordered_map<std::string, MaterialHandle> m_MaterialNameToHandleMap; // Assumes material names from file are somewhat unique
        std::unordered_map<std::string, TextureHandle> m_TexturePathToHandleMap;

        // --- Handles to Default Assets ---
        TextureHandle m_DefaultWhiteTexture = InvalidTextureHandle;
        MaterialHandle m_DefaultMaterial = InvalidMaterialHandle;
    };

} // namespace VulkEng

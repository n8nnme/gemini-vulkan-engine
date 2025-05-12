#pragma once

#include "Texture.h" // For TextureHandle and InvalidTextureHandle
#include <glm/glm.hpp>
#include <vulkan/vulkan.h> // For VkDescriptorSet
#include <string>
#include <vector> // Potentially for multiple textures of the same type

namespace VulkEng {

    // Represents material properties for rendering a mesh.
    // This can include texture handles, color factors, and PBR parameters.
    struct Material {
        std::string name = "DefaultMaterial"; // A human-readable name for the material

        // --- Common Texture Handles ---
        // These handles refer to textures managed by the AssetManager.
        TextureHandle diffuseTexture = InvalidTextureHandle;    // Also known as Albedo or Base Color map
        TextureHandle normalTexture = InvalidTextureHandle;     // Normal map for surface detail
        // TextureHandle specularTexture = InvalidTextureHandle; // For older specular/gloss workflows
        // TextureHandle emissiveTexture = InvalidTextureHandle;   // For self-illuminating parts

        // --- PBR (Physically Based Rendering) Texture Handles & Factors ---
        // Often, metallic and roughness are packed into a single texture (e.g., GLTF standard).
        // Blue channel for metallic, Green channel for roughness.
        TextureHandle metallicRoughnessTexture = InvalidTextureHandle;
        TextureHandle ambientOcclusionTexture = InvalidTextureHandle; // AO map

        // --- Color Factors & Scalar Properties ---
        // These are used if corresponding textures are not provided, or to modulate texture values.
        glm::vec4 baseColorFactor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // RGBA, default white
        float metallicFactor = 1.0f;    // Range [0, 1]. Default 1 for dielectrics if no map, or based on model.
                                        // For metals, this is often 1.0.
        float roughnessFactor = 1.0f;   // Range [0, 1]. Default 1 for rough if no map.
        glm::vec3 emissiveFactor = glm::vec3(0.0f, 0.0f, 0.0f); // Emissive color if no emissive map

        // Alpha properties
        enum class AlphaMode { OPAQUE, MASK, BLEND };
        AlphaMode alphaMode = AlphaMode::OPAQUE;
        float alphaCutoff = 0.5f; // Used for AlphaMode::MASK

        bool doubleSided = false; // Whether back-face culling should be disabled for this material

        // --- Vulkan Specific ---
        // Descriptor set allocated by AssetManager specifically for this material's textures.
        // This set would typically be bound at Set Index 1 in the pipeline layout.
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        // Optional: Shader identifier or parameters to select/configure shader variations
        // std::string shaderName = "PBR_MetallicRoughness";

        // Default constructor
        Material() = default;

        // Constructor for convenience
        explicit Material(const std::string& matName) : name(matName) {}
    };

    // Handle type for referencing materials managed by the AssetManager.
    using MaterialHandle = size_t;
    // A special handle value to indicate an invalid or unassigned material.
    const MaterialHandle InvalidMaterialHandle = static_cast<MaterialHandle>(-1);

} // namespace VulkEng





// 12 May 20:01 | yeah
// #pragma once
// 
// #include "Texture.h" // Needs TextureHandle
// #include <glm/glm.hpp>
// #include <vulkan/vulkan.h> // For VkDescriptorSet
// #include <string>
// 
// namespace VulkEng {
// 
//     struct Material {
//         std::string name;
//         TextureHandle diffuseTexture = InvalidTextureHandle;
//         glm::vec4 baseColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // Default white
// 
//         // Descriptor set allocated for this material (holds its texture sampler)
//         VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
// 
//         // Add other properties like metallic, roughness, normal map texture handle etc.
//         // float metallic = 0.0f;
//         // float roughness = 1.0f;
//         // TextureHandle normalTexture = InvalidTextureHandle;
//         // TextureHandle metallicRoughnessTexture = InvalidTextureHandle;
//         // TextureHandle emissiveTexture = InvalidTextureHandle;
//         // glm::vec3 emissiveFactor = glm::vec3(0.0f);
//     };
// 
//      using MaterialHandle = size_t;
//      const MaterialHandle InvalidMaterialHandle = static_cast<MaterialHandle>(-1);
// 
// } // namespace VulkEng

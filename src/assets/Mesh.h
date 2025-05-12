#pragma once

#include "Material.h" // For MaterialHandle

#include <glm/glm.hpp>
#include <vulkan/vulkan.h> // For Vulkan types (VkBuffer, VkDeviceSize, etc.)
#include <vector>
#include <memory>    // For std::shared_ptr (used for VulkanBuffer)
#include <string>    // For potential mesh name

namespace VulkEng {

    // Forward declaration of the VulkanBuffer wrapper class
    class VulkanBuffer;

    // Defines the structure of a single vertex.
    // This layout must match the vertex input binding and attribute descriptions
    // provided to the graphics pipeline and the input layout in the vertex shader.
    struct Vertex {
        glm::vec3 position;  // Vertex position (x, y, z)
        glm::vec3 normal;    // Vertex normal (for lighting)
        glm::vec2 texCoord;  // Texture coordinates (u, v)
        glm::vec4 color;     // Vertex color (RGBA, optional, can modulate texture)
        glm::vec3 tangent;   // Vertex tangent (for normal mapping, bitangent can be derived)
        // glm::vec3 bitangent; // Optional: Can be calculated from normal and tangent in shader

        // Static methods to get Vulkan descriptions for this vertex layout.
        // These are used when creating the graphics pipeline.
        static VkVertexInputBindingDescription getBindingDescription();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    };

    // Holds CPU-side mesh data as loaded from a model file (e.g., by Assimp).
    // This data is then used to create GPU buffers (VulkanBuffer).
    struct MeshData {
        std::string name;                      // Optional name for the mesh part
        std::vector<Vertex> vertices;          // List of vertices
        std::vector<uint32_t> indices;         // List of indices defining triangles (or other primitives)
        unsigned int materialIndex = 0;        // Index into the model's original material list (used by ModelLoader/AssetManager)
                                               // This is an index from the source file (e.g. Assimp's material array).
                                               // AssetManager will convert this to an engine MaterialHandle.
        // Optional: Bounding box calculated on CPU
        // glm::vec3 minBounds, maxBounds;
    };

    // Represents a mesh that has been uploaded to the GPU.
    // It holds handles to Vulkan buffers for vertices and indices,
    // and a handle to the material used for rendering this mesh.
    struct Mesh {
        std::string name; // Optional name for the mesh part, can be copied from MeshData

        // Vertex Buffer
        std::shared_ptr<VulkanBuffer> vertexBuffer; // GPU buffer containing vertex data
        VkDeviceSize vertexBufferOffset = 0;        // Offset into the vertexBuffer (if using a shared buffer)
        uint32_t vertexCount = 0;                   // Number of vertices in this mesh

        // Index Buffer
        std::shared_ptr<VulkanBuffer> indexBuffer;  // GPU buffer containing index data
        VkDeviceSize indexBufferOffset = 0;         // Offset into the indexBuffer
        uint32_t indexCount = 0;                    // Number of indices in this mesh

        // Material
        MaterialHandle material = InvalidMaterialHandle; // Handle to the Material used by this mesh

        // Optional: Bounding box (Axis-Aligned Bounding Box) for frustum culling
        // This would typically be calculated from vertex data.
        // struct AABB {
        //     glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
        //     glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());
        // } boundingBox;
    };

} // namespace VulkEng


// 12 May 20:02 | ...
// #pragma once
// 
// #include <glm/glm.hpp>
// #include <vulkan/vulkan.h>
// #include <vector>
// #include <memory>
// #include "Material.h" // Include MaterialHandle definition
// 
// namespace VulkEng {
// 
//     struct Vertex {
//         glm::vec3 pos;
//         glm::vec3 normal;
//         glm::vec2 texCoord;
//         glm::vec4 color;
//         glm::vec3 tangent; // For normal mapping
// 
//         static VkVertexInputBindingDescription getBindingDescription();
//         static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
//     };
// 
//     struct MeshData { // CPU-side data loaded from file
//         std::vector<Vertex> vertices;
//         std::vector<uint32_t> indices;
//         unsigned int materialIndex = 0; // Index into model's material list
//     };
// 
//     class VulkanBuffer; // Forward declaration
// 
//     struct Mesh { // GPU-side representation
//         std::shared_ptr<VulkanBuffer> vertexBuffer;
//         VkDeviceSize vertexBufferOffset = 0;
//         uint32_t vertexCount = 0;
// 
//         std::shared_ptr<VulkanBuffer> indexBuffer;
//         VkDeviceSize indexBufferOffset = 0;
//         uint32_t indexCount = 0;
// 
//         MaterialHandle material = InvalidMaterialHandle; // Handle to the material used by this mesh
//         // AABB boundingBox; // For culling
//     };
// 
// } // namespace VulkEng

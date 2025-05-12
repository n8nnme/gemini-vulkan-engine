#include "Mesh.h"
#include <vector> // Required for std::vector
#include <array>  // Can be useful for attribute descriptions if fixed size, but vector is flexible

namespace VulkEng {

    // --- Vertex Struct Static Method Implementations ---

    // Provides the binding description for the Vertex struct.
    // This tells Vulkan how vertex data is laid out per binding point.
    // We are using a single binding point (binding 0) for all vertex attributes.
    VkVertexInputBindingDescription Vertex::getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0; // The index of the binding in the array of bindings
        bindingDescription.stride = sizeof(Vertex); // The byte distance between consecutive elements in the buffer
        // VK_VERTEX_INPUT_RATE_VERTEX:   Move to the next data entry after each vertex.
        // VK_VERTEX_INPUT_RATE_INSTANCE: Move to the next data entry after each instance (for instanced rendering).
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    // Provides the attribute descriptions for the Vertex struct.
    // This tells Vulkan how to interpret the data for each attribute within a vertex.
    // Each attribute description corresponds to a `layout(location = X)` in the vertex shader.
    std::vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        attributeDescriptions.resize(5); // We have 5 attributes: position, normal, texCoord, color, tangent

        // Attribute 0: Position (matches layout(location = 0) in shader)
        attributeDescriptions[0].binding = 0; // Data comes from binding 0 (defined in getBindingDescription)
        attributeDescriptions[0].location = 0; // Shader input location
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; // Format is vec3 of floats
        attributeDescriptions[0].offset = offsetof(Vertex, position); // Byte offset of this attribute within the Vertex struct

        // Attribute 1: Normal (matches layout(location = 1) in shader)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        // Attribute 2: Texture Coordinates (matches layout(location = 2) in shader)
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT; // vec2
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        // Attribute 3: Color (matches layout(location = 3) in shader)
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT; // vec4
        attributeDescriptions[3].offset = offsetof(Vertex, color);

        // Attribute 4: Tangent (matches layout(location = 4) in shader)
        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        attributeDescriptions[4].offset = offsetof(Vertex, tangent);

        return attributeDescriptions;
    }

} // namespace VulkEng



// 12 May 20:04 | 0
// #include "Mesh.h"
// #include <array> // Not strictly needed here but often used with fixed-size attribute arrays
// 
// namespace VulkEng {
// 
//     VkVertexInputBindingDescription Vertex::getBindingDescription() {
//         VkVertexInputBindingDescription bindingDescription{};
//         bindingDescription.binding = 0;
//         bindingDescription.stride = sizeof(Vertex);
//         bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
//         return bindingDescription;
//     }
// 
//     std::vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions() {
//         std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
//         attributeDescriptions.resize(5); // pos, normal, texCoord, color, tangent
// 
//         // Position
//         attributeDescriptions[0].binding = 0;
//         attributeDescriptions[0].location = 0;
//         attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
//         attributeDescriptions[0].offset = offsetof(Vertex, pos);
// 
//         // Normal
//         attributeDescriptions[1].binding = 0;
//         attributeDescriptions[1].location = 1;
//         attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
//         attributeDescriptions[1].offset = offsetof(Vertex, normal);
// 
//         // Texture Coordinate
//         attributeDescriptions[2].binding = 0;
//         attributeDescriptions[2].location = 2;
//         attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
//         attributeDescriptions[2].offset = offsetof(Vertex, texCoord);
// 
//         // Color
//         attributeDescriptions[3].binding = 0;
//         attributeDescriptions[3].location = 3;
//         attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
//         attributeDescriptions[3].offset = offsetof(Vertex, color);
// 
//         // Tangent
//         attributeDescriptions[4].binding = 0;
//         attributeDescriptions[4].location = 4;
//         attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
//         attributeDescriptions[4].offset = offsetof(Vertex, tangent);
// 
//         return attributeDescriptions;
//     }
// 
// } // namespace VulkEng

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint> // For uint32_t, size_t
#include <stdexcept> // For std::runtime_error

// It's good practice to forward declare if VulkanContext is only needed for pointers/references
// but here we might need its device/physicalDevice for some inline helpers or default args,
// so including might be simpler for this scope.
// For now, assume it's forward declared or included where VulkanBuffer is used.
namespace VulkEng {
    class VulkanContext;
}

namespace VulkEng {

    // Wrapper class for Vulkan VkBuffer and its associated VkDeviceMemory.
    // This example uses manual memory management. For production, consider VMA (Vulkan Memory Allocator).
    class VulkanBuffer {
    public:
        // Constructor for creating a buffer.
        // - context: Reference to the initialized VulkanContext.
        // - instanceSize: Size of a single element/instance in the buffer (e.g., sizeof(Vertex) or sizeof(UBO)).
        // - instanceCount: Number of elements/instances the buffer will hold.
        // - usageFlags: VkBufferUsageFlags specifying how the buffer will be used (e.g., vertex, index, uniform, staging).
        // - memoryPropertyFlags: VkMemoryPropertyFlags specifying desired memory properties (e.g., host-visible, device-local).
        // - minOffsetAlignment: Minimum alignment requirement for dynamic uniform buffers (usually 1 if not dynamic UBO).
        VulkanBuffer(
            VulkanContext& context,
            VkDeviceSize instanceSize,
            uint32_t instanceCount,
            VkBufferUsageFlags usageFlags,
            VkMemoryPropertyFlags memoryPropertyFlags,
            VkDeviceSize minOffsetAlignment = 1 // Default to 1 (no special alignment beyond instanceSize)
        );

        // Destructor: Frees the VkDeviceMemory and destroys the VkBuffer.
        ~VulkanBuffer();

        // --- Rule of Five: Prevent copying, allow moving (or implement properly) ---
        VulkanBuffer(const VulkanBuffer&) = delete;
        VulkanBuffer& operator=(const VulkanBuffer&) = delete;

        // Move constructor and assignment (optional, implement if needed for specific ownership patterns)
        VulkanBuffer(VulkanBuffer&& other) noexcept;
        VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;


        // --- Memory Mapping ---
        // Maps a region of the buffer's memory into host-accessible address space.
        // Only valid for buffers created with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT.
        // `size` and `offset` define the region to map. VK_WHOLE_SIZE maps the entire buffer.
        VkResult Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        // Unmaps previously mapped memory. Must be called if Map was successful.
        void Unmap();

        // --- Data Transfer ---
        // Writes data directly to the mapped memory region.
        // Assumes the buffer is already mapped or handles mapping internally if persistently mapped.
        // If not persistently mapped and not HOST_COHERENT, requires Flush after writing.
        void WriteToBuffer(const void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        // Flushes a mapped memory range to make host writes visible to the device.
        // Only necessary for HOST_VISIBLE memory that is NOT HOST_COHERENT.
        // `size` and `offset` define the region to flush.
        VkResult Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;

        // Invalidates a mapped memory range to make device writes visible to the host.
        // Only necessary for HOST_VISIBLE memory that is NOT HOST_COHERENT and is read by host.
        VkResult Invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;


        // --- Descriptors ---
        // Returns a VkDescriptorBufferInfo struct for this buffer, used when updating descriptor sets.
        // `size` and `offset` can specify a sub-region of the buffer.
        VkDescriptorBufferInfo GetDescriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;


        // --- Accessors ---
        VkBuffer GetBuffer() const { return m_Buffer; }
        VkDeviceMemory GetMemory() const { return m_Memory; } // Exposing memory handle directly
        void* GetMappedMemory() const { return m_MappedMemory; } // Returns nullptr if not mapped

        uint32_t GetInstanceCount() const { return m_InstanceCount; }
        VkDeviceSize GetInstanceSize() const { return m_InstanceSize; }     // Size of one element (unaligned)
        VkDeviceSize GetAlignmentSize() const { return m_AlignmentSize; }   // Size of one element (aligned)
        VkBufferUsageFlags GetUsageFlags() const { return m_UsageFlags; }
        VkMemoryPropertyFlags GetMemoryPropertyFlags() const { return m_MemoryPropertyFlags; }
        VkDeviceSize GetBufferSize() const { return m_BufferSize; }       // Total size of the buffer (alignedSize * count)


    private:
        // Helper to calculate aligned size for dynamic UBOs or other aligned data.
        static VkDeviceSize CalculateAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

        VulkanContext& m_Context; // Store reference to Vulkan context for device access

        VkBuffer m_Buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_Memory = VK_NULL_HANDLE; // Handle to the allocated device memory

        void* m_MappedMemory = nullptr; // Pointer to mapped host memory (if HOST_VISIBLE and mapped)
        VkDeviceSize m_BufferSize = 0;  // Total size of the allocated buffer
        uint32_t m_InstanceCount = 0;
        VkDeviceSize m_InstanceSize = 0;  // Requested size of a single instance/element
        VkDeviceSize m_AlignmentSize = 0; // Actual size of an instance after alignment
        VkBufferUsageFlags m_UsageFlags = 0;
        VkMemoryPropertyFlags m_MemoryPropertyFlags = 0;
    };

} // namespace VulkEng

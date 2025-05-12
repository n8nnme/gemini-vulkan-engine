#include "Buffer.h"
#include "graphics/VulkanContext.h" // Needs full definition for device, physicalDevice
#include "graphics/VulkanUtils.h"   // For VK_CHECK and findMemoryType
#include "core/Log.h"

#include <stdexcept> // For std::runtime_error
#include <cstring>   // For memcpy

namespace VulkEng {

    // --- Static Helper for Alignment Calculation ---
    VkDeviceSize VulkanBuffer::CalculateAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment) {
        if (minOffsetAlignment > 0) {
            // Vulkan formula for alignment: (size + alignment - 1) & ~(alignment - 1)
            return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
        }
        return instanceSize; // No special alignment needed beyond the instance size itself
    }

    // --- Constructor ---
    VulkanBuffer::VulkanBuffer(
        VulkanContext& context,
        VkDeviceSize instanceSize,
        uint32_t instanceCount,
        VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags memoryPropertyFlags,
        VkDeviceSize minOffsetAlignment /*= 1*/)
        : m_Context(context),
          m_Buffer(VK_NULL_HANDLE),
          m_Memory(VK_NULL_HANDLE),
          m_MappedMemory(nullptr),
          m_InstanceCount(instanceCount),
          m_InstanceSize(instanceSize),
          m_UsageFlags(usageFlags),
          m_MemoryPropertyFlags(memoryPropertyFlags)
    {
        if (instanceCount == 0 || instanceSize == 0) {
            VKENG_ERROR("VulkanBuffer: Instance count or size cannot be zero.");
            // Or throw std::invalid_argument
            // For now, proceed but buffer will be invalid.
            m_BufferSize = 0;
            m_AlignmentSize = 0;
            return;
        }
        if (m_Context.device == VK_NULL_HANDLE) { // Check if context is valid
            VKENG_ERROR("VulkanBuffer: Cannot create buffer with a null Vulkan device in context!");
            throw std::runtime_error("Null device for VulkanBuffer creation.");
        }


        m_AlignmentSize = CalculateAlignment(instanceSize, minOffsetAlignment);
        m_BufferSize = m_AlignmentSize * instanceCount;

        // VKENG_TRACE("Creating VulkanBuffer: Size={}, Count={}, InstSize={}, AlignedInstSize={}, TotalSize={}",
        //             instanceSize, instanceCount, m_InstanceSize, m_AlignmentSize, m_BufferSize);

        // --- Create Buffer Object ---
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = m_BufferSize;
        bufferInfo.usage = usageFlags;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Assume exclusive unless explicitly specified

        VK_CHECK(vkCreateBuffer(m_Context.device, &bufferInfo, nullptr, &m_Buffer));

        // --- Get Memory Requirements ---
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_Context.device, m_Buffer, &memRequirements);

        // --- Allocate Memory ---
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size; // Use size from requirements
        // Find a memory type that is suitable for the buffer and has the requested properties
        allocInfo.memoryTypeIndex = Utils::findMemoryType(
            m_Context.physicalDevice,
            memRequirements.memoryTypeBits, // Filter from requirements
            memoryPropertyFlags           // Desired properties
        );

        // Optional: Enable VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT if using buffer device addresses (Vulkan 1.2+)
        // if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        //    allocInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        // }

        VK_CHECK(vkAllocateMemory(m_Context.device, &allocInfo, nullptr, &m_Memory));

        // --- Bind Memory to Buffer ---
        VK_CHECK(vkBindBufferMemory(m_Context.device, m_Buffer, m_Memory, 0)); // 0 is the offset within the memory allocation
    }

    // --- Destructor ---
    VulkanBuffer::~VulkanBuffer() {
        // Unmap memory if it was mapped
        if (m_MappedMemory) {
            Unmap(); // Calls vkUnmapMemory
        }

        // Destroy buffer and free memory only if they were successfully created
        if (m_Buffer != VK_NULL_HANDLE && m_Context.device != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_Context.device, m_Buffer, nullptr);
            m_Buffer = VK_NULL_HANDLE;
        }
        if (m_Memory != VK_NULL_HANDLE && m_Context.device != VK_NULL_HANDLE) {
            vkFreeMemory(m_Context.device, m_Memory, nullptr);
            m_Memory = VK_NULL_HANDLE;
        }
    }

    // --- Move Constructor ---
    VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
        : m_Context(other.m_Context), // Copy context reference
          m_Buffer(other.m_Buffer),
          m_Memory(other.m_Memory),
          m_MappedMemory(other.m_MappedMemory),
          m_BufferSize(other.m_BufferSize),
          m_InstanceCount(other.m_InstanceCount),
          m_InstanceSize(other.m_InstanceSize),
          m_AlignmentSize(other.m_AlignmentSize),
          m_UsageFlags(other.m_UsageFlags),
          m_MemoryPropertyFlags(other.m_MemoryPropertyFlags)
    {
        // Nullify other's resources to prevent double deletion
        other.m_Buffer = VK_NULL_HANDLE;
        other.m_Memory = VK_NULL_HANDLE;
        other.m_MappedMemory = nullptr;
        other.m_BufferSize = 0;
        other.m_InstanceCount = 0;
        // Other POD members don't need explicit nulling if other is destructed properly
    }

    // --- Move Assignment Operator ---
    VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept {
        if (this != &other) { // Prevent self-assignment
            // Release existing resources
            if (m_MappedMemory) Unmap();
            if (m_Buffer != VK_NULL_HANDLE && m_Context.device != VK_NULL_HANDLE) vkDestroyBuffer(m_Context.device, m_Buffer, nullptr);
            if (m_Memory != VK_NULL_HANDLE && m_Context.device != VK_NULL_HANDLE) vkFreeMemory(m_Context.device, m_Memory, nullptr);

            // Steal resources from other
            // m_Context = other.m_Context; // Context reference remains the same (or needs careful handling if it can change)
                                          // Assuming m_Context reference is correctly set for the *new* object location.
                                          // If this object was default constructed then moved into, m_Context needs to be valid.
                                          // The current constructor requires a valid context, so this should be fine.
            m_Buffer = other.m_Buffer;
            m_Memory = other.m_Memory;
            m_MappedMemory = other.m_MappedMemory;
            m_BufferSize = other.m_BufferSize;
            m_InstanceCount = other.m_InstanceCount;
            m_InstanceSize = other.m_InstanceSize;
            m_AlignmentSize = other.m_AlignmentSize;
            m_UsageFlags = other.m_UsageFlags;
            m_MemoryPropertyFlags = other.m_MemoryPropertyFlags;

            // Nullify other's resources
            other.m_Buffer = VK_NULL_HANDLE;
            other.m_Memory = VK_NULL_HANDLE;
            other.m_MappedMemory = nullptr;
            other.m_BufferSize = 0;
            other.m_InstanceCount = 0;
        }
        return *this;
    }


    // --- Memory Mapping ---
    VkResult VulkanBuffer::Map(VkDeviceSize size, VkDeviceSize offset) {
        if (m_Buffer == VK_NULL_HANDLE || m_Memory == VK_NULL_HANDLE) {
            VKENG_ERROR("VulkanBuffer::Map: Buffer or memory is null. Cannot map.");
            return VK_ERROR_INITIALIZATION_FAILED; // Or some other appropriate error
        }
        if (!(m_MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            VKENG_ERROR("VulkanBuffer::Map: Attempting to map non-host-visible buffer memory!");
            // Or throw std::runtime_error
            return VK_ERROR_MEMORY_MAP_FAILED;
        }
        if (m_MappedMemory != nullptr) {
            VKENG_WARN("VulkanBuffer::Map: Buffer memory is already mapped.");
            return VK_SUCCESS; // Already mapped, consider this success or an error
        }

        return vkMapMemory(m_Context.device, m_Memory, offset, size, 0, &m_MappedMemory);
    }

    void VulkanBuffer::Unmap() {
        if (m_MappedMemory) {
             if (m_Context.device != VK_NULL_HANDLE && m_Memory != VK_NULL_HANDLE) { // Check validity
                vkUnmapMemory(m_Context.device, m_Memory);
            }
            m_MappedMemory = nullptr;
        }
    }

    // --- Data Transfer ---
    void VulkanBuffer::WriteToBuffer(const void* data, VkDeviceSize size, VkDeviceSize offset) {
        if (m_Buffer == VK_NULL_HANDLE) {
            VKENG_ERROR("VulkanBuffer::WriteToBuffer: Buffer is null.");
            return;
        }
        if (!(m_MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            VKENG_ERROR("VulkanBuffer::WriteToBuffer: Cannot write directly to non-host-visible buffer. Use staging.");
            throw std::runtime_error("Writing to non-host-visible buffer without staging.");
        }
        if (!data) {
            VKENG_ERROR("VulkanBuffer::WriteToBuffer: Source data pointer is null.");
            return;
        }

        bool needsTempMap = (m_MappedMemory == nullptr);
        if (needsTempMap) {
            VkResult mapResult = Map(size == VK_WHOLE_SIZE ? m_BufferSize : size, offset);
            if (mapResult != VK_SUCCESS) {
                VKENG_ERROR("VulkanBuffer::WriteToBuffer: Failed to map memory for writing. Error: {}", mapResult);
                return;
            }
        }

        // Ensure mapped memory is valid after potential map call
        if (!m_MappedMemory) {
             VKENG_ERROR("VulkanBuffer::WriteToBuffer: Memory not mapped after Map() call.");
             return;
        }


        char* mappedDataOffset = static_cast<char*>(m_MappedMemory) + offset;
        if (size == VK_WHOLE_SIZE) {
            if (offset > 0) { // If offset given with WHOLE_SIZE, adjust actual size
                memcpy(mappedDataOffset, data, m_BufferSize - offset);
            } else {
                memcpy(m_MappedMemory, data, m_BufferSize);
            }
        } else {
            memcpy(mappedDataOffset, data, size);
        }

        // If memory is not HOST_COHERENT, it needs to be flushed for device to see writes.
        if (!(m_MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            Flush(size, offset); // Flush the written region
        }

        if (needsTempMap) {
            Unmap();
        }
    }

    VkResult VulkanBuffer::Flush(VkDeviceSize size, VkDeviceSize offset) const {
        if (m_Memory == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
        // Only flush if host-visible and not host-coherent
        if (!(m_MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ||
            (m_MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            return VK_SUCCESS; // No flush needed or possible
        }

        VkMappedMemoryRange mappedRange{};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = m_Memory;
        mappedRange.offset = offset;
        mappedRange.size = (size == VK_WHOLE_SIZE) ? m_BufferSize - offset : size;
        return vkFlushMappedMemoryRanges(m_Context.device, 1, &mappedRange);
    }

    VkResult VulkanBuffer::Invalidate(VkDeviceSize size, VkDeviceSize offset) const {
        if (m_Memory == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
        // Only invalidate if host-visible and not host-coherent
        if (!(m_MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ||
            (m_MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            return VK_SUCCESS; // No invalidate needed or possible
        }

        VkMappedMemoryRange mappedRange{};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = m_Memory;
        mappedRange.offset = offset;
        mappedRange.size = (size == VK_WHOLE_SIZE) ? m_BufferSize - offset : size;
        return vkInvalidateMappedMemoryRanges(m_Context.device, 1, &mappedRange);
    }

    // --- Descriptors ---
    VkDescriptorBufferInfo VulkanBuffer::GetDescriptorInfo(VkDeviceSize size, VkDeviceSize offset) const {
        if (m_Buffer == VK_NULL_HANDLE) {
            VKENG_ERROR("VulkanBuffer::GetDescriptorInfo: Buffer is null.");
            // Return a potentially invalid descriptor or throw.
            // Let's return something that might cause a Vulkan error if used, to highlight the issue.
            return {VK_NULL_HANDLE, 0, 0};
        }
        return {
            m_Buffer,
            offset,
            (size == VK_WHOLE_SIZE) ? m_BufferSize - offset : size
        };
    }

    // Accessors are inline in the header.

} // namespace VulkEng

#include "VulkanContext.h"
#include "VulkanUtils.h" // For VK_CHECK and other utility functions
#include "core/Log.h"    // For logging
#include "core/Window.h"   // For m_Window interaction

#include <vector>
#include <stdexcept> // For std::runtime_error
#include <set>       // For checking required extensions
#include <cstring>   // For strcmp (used in extension/layer checking)
#include <algorithm> // For std::find

// --- Configuration ---
#ifdef NDEBUG
const bool enableValidationLayersGlobal = false; // Use a distinct name
#else
const bool enableValidationLayersGlobal = true;
#endif

const std::vector<const char*> globalValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Base required device extensions
const std::vector<const char*> globalDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
    // Add more common extensions if generally needed, e.g.,
    // VK_KHR_MAINTENANCE1_EXTENSION_NAME, // Often useful for negative viewport heights
    // VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, // If using Vulkan 1.3 dynamic rendering
};


// --- Local Helper Functions for Debug Messenger ---
// (Kept local to this .cpp as they directly use vkGetInstanceProcAddr)
VkResult CreateDebugUtilsMessengerEXT_Local(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT_Local(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

// The actual callback function
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback_Local(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    // Log warnings and errors. Info and Verbose can be very spammy.
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            VKENG_ERROR("[Vulkan Validation Layer] ID: {} Name: {}\n  Message: {}",
                        pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "N/A",
                        pCallbackData->messageIdNumber,
                        pCallbackData->pMessage);
        } else { // Warning
            VKENG_WARN("[Vulkan Validation Layer] ID: {} Name: {}\n  Message: {}",
                       pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "N/A",
                       pCallbackData->messageIdNumber,
                       pCallbackData->pMessage);
        }
    }
    return VK_FALSE; // Vulkan spec requires returning VK_FALSE
}


namespace VulkEng {

    VulkanContext::VulkanContext(Window& window, bool skipVulkanInit /*= false*/)
        : m_Window(window),
          instance(VK_NULL_HANDLE), debugMessenger(VK_NULL_HANDLE), surface(VK_NULL_HANDLE),
          physicalDevice(VK_NULL_HANDLE), device(VK_NULL_HANDLE),
          graphicsQueue(VK_NULL_HANDLE), presentQueue(VK_NULL_HANDLE),
          graphicsQueueFamily(UINT32_MAX), presentQueueFamily(UINT32_MAX),
          mainRenderPass(VK_NULL_HANDLE), imageCount(0), minImageCount(0)
          // physicalDeviceProperties and physicalDeviceFeatures are default-initialized
    {
        if (skipVulkanInit) {
            VKENG_WARN("VulkanContext: Skipping Vulkan API Initialization for Dummy/Null instance!");
            return; // Exit early, all members remain in their initial (mostly null) state.
        }

        VKENG_INFO("Initializing Vulkan Context...");
        CreateInstance(); // Throws on failure
        SetupDebugMessenger(); // Optional, based on validation layers
        m_Window.CreateWindowSurface(instance, &surface); // Can throw if window is null or surface creation fails
        if (surface == VK_NULL_HANDLE && m_Window.GetGLFWwindow() != nullptr) { // Only error if window was real
            throw std::runtime_error("Vulkan surface creation failed for a valid window.");
        }
        PickPhysicalDevice(); // Throws on failure
        CreateLogicalDevice(); // Throws on failure
        VKENG_INFO("Vulkan Context Initialized Successfully.");
    }

    VulkanContext::~VulkanContext() {
        VKENG_INFO("Destroying Vulkan Context...");
        // Resources are destroyed in reverse order of creation.
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        if (surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) { // Surface needs instance
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
        if (enableValidationLayersGlobal && debugMessenger != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
            DestroyDebugUtilsMessengerEXT_Local(instance, debugMessenger, nullptr);
            debugMessenger = VK_NULL_HANDLE;
        }
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }
        VKENG_INFO("Vulkan Context Destroyed.");
    }

    void VulkanContext::CreateInstance() {
        VKENG_INFO("Creating Vulkan Instance...");
        if (enableValidationLayersGlobal && !CheckValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "VulkanEngine";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.pEngineName = "VulkEng";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3; // Target Vulkan 1.3

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto requiredExtensions = GetRequiredInstanceExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();
        VKENG_INFO("Required Instance Extensions:");
        for(const char* extName : requiredExtensions) VKENG_INFO("  - {}", extName);


        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayersGlobal) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(globalValidationLayers.size());
            createInfo.ppEnabledLayerNames = globalValidationLayers.data();
            PopulateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = &debugCreateInfo; // Attach debug messenger for instance create/destroy
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        // Handle macOS Portability Extension for instance creation flags
        #ifdef __APPLE__
        bool portabilityEnumerationPresent = false;
        for(const char* ext : requiredExtensions) {
            if (strcmp(ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
                portabilityEnumerationPresent = true;
                break;
            }
        }
        if (portabilityEnumerationPresent) {
            createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            VKENG_INFO("Enabling VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR for macOS.");
        }
        #endif

        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
        VKENG_INFO("Vulkan Instance Created.");
    }

    bool VulkanContext::CheckValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : globalValidationLayers) {
            bool layerFound = false;
            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound) {
                VKENG_ERROR("Validation Layer Not Found: {}", layerName);
                return false;
            }
        }
        VKENG_INFO("All requested validation layers are available.");
        return true;
    }

    std::vector<const char*> VulkanContext::GetRequiredInstanceExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayersGlobal) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        #ifdef __APPLE__
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            // VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME might be implicitly required by portability
            // or other extensions. Check device extension support later.
        #endif
        return extensions;
    }

    void VulkanContext::SetupDebugMessenger() {
        if (!enableValidationLayersGlobal || !instance) return; // Check if instance is valid
        VKENG_INFO("Setting up Debug Messenger...");
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        PopulateDebugMessengerCreateInfo(createInfo);
        VK_CHECK(CreateDebugUtilsMessengerEXT_Local(instance, &createInfo, nullptr, &debugMessenger));
        VKENG_INFO("Debug Messenger Setup Complete.");
    }

    void VulkanContext::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        // Uncomment for more verbose logging:
        // createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        // createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugCallback_Local;
        createInfo.pUserData = nullptr;
    }

    void VulkanContext::PickPhysicalDevice() {
        VKENG_INFO("Picking Physical Device...");
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        VKENG_INFO("Available Physical Devices ({}):", deviceCount);
        for (const auto& currentDevice : devices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(currentDevice, &props);
            VKENG_INFO("  - Name: {}, Type: {}, ID: {}", props.deviceName, props.deviceType, props.deviceID);
            if (IsDeviceSuitable(currentDevice)) {
                physicalDevice = currentDevice;
                vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties); // Cache properties
                vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);   // Cache features
                VKENG_INFO("Selected Physical Device: {}", physicalDeviceProperties.deviceName);
                return; // Found a suitable device
            }
        }
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    bool VulkanContext::IsDeviceSuitable(VkPhysicalDevice currentDevice) {
        QueueFamilyIndices indices = FindQueueFamilies(currentDevice);
        if (!indices.IsComplete()) {
             VKENG_INFO("Device skipped: Incomplete queue families.");
             return false;
        }

        bool extensionsSupported = CheckDeviceExtensionSupport(currentDevice);
        if (!extensionsSupported) {
            VKENG_INFO("Device skipped: Does not support required device extensions.");
            return false;
        }

        bool swapChainAdequate = false;
        if (surface != VK_NULL_HANDLE) { // Only check swapchain if a real surface exists
            SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(currentDevice);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            if (!swapChainAdequate) VKENG_INFO("Device skipped: Inadequate swap chain support.");
        } else {
            swapChainAdequate = true; // No surface, so swapchain support is not a blocker
        }


        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(currentDevice, &supportedFeatures);
        bool featuresSupported = supportedFeatures.samplerAnisotropy; // Example feature
        // Add other feature checks: featuresSupported = featuresSupported && supportedFeatures.geometryShader;
        if (!featuresSupported) VKENG_INFO("Device skipped: Lacks required features (e.g., samplerAnisotropy).");

        // Check for Blit support
        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(currentDevice, VK_FORMAT_R8G8B8A8_SRGB, &formatProps);
        bool blitSupport = (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) &&
                           (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) &&
                           (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
        if (!blitSupport) VKENG_INFO("Device skipped: Format R8G8B8A8_SRGB lacks full blit/filter support for mipmapping.");


        return indices.IsComplete() && extensionsSupported && swapChainAdequate && featuresSupported && blitSupport;
    }

    QueueFamilyIndices VulkanContext::FindQueueFamilies(VkPhysicalDevice currentDevice) {
        QueueFamilyIndices indices;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(currentDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(currentDevice, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }
            if (surface != VK_NULL_HANDLE) { // Only check present if we have a surface
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(currentDevice, i, surface, &presentSupport);
                if (presentSupport) {
                    indices.presentFamily = i;
                }
            } else { // If no surface (dummy context), assume graphics queue can present or pick first one
                indices.presentFamily = indices.graphicsFamily;
            }

            if (indices.IsComplete()) break;
            i++;
        }
        return indices;
    }

    bool VulkanContext::CheckDeviceExtensionSupport(VkPhysicalDevice currentDevice) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(currentDevice, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(currentDevice, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(globalDeviceExtensions.begin(), globalDeviceExtensions.end());

        // For macOS, VK_KHR_portability_subset is required if available on the device
        #ifdef __APPLE__
        bool needsPortabilitySubset = false;
        for (const auto& ext : availableExtensions) {
            if (strcmp(ext.extensionName, "VK_KHR_portability_subset") == 0) {
                needsPortabilitySubset = true;
                break;
            }
        }
        if (needsPortabilitySubset) {
            requiredExtensions.insert("VK_KHR_portability_subset");
            VKENG_INFO("Device requires VK_KHR_portability_subset extension.");
        }
        #endif

        VKENG_INFO("Required Device Extensions for consideration:");
        for(const auto& reqExt : requiredExtensions) VKENG_INFO("  - {}", reqExt);
        VKENG_INFO("Available Device Extensions:");
        for(const auto& availExt : availableExtensions) VKENG_INFO("  - {}", availExt.extensionName);


        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        if (!requiredExtensions.empty()) {
            VKENG_WARN("Missing required device extensions:");
            for(const auto& missing : requiredExtensions) VKENG_WARN("  - {}", missing);
        }
        return requiredExtensions.empty();
    }

    SwapChainSupportDetails VulkanContext::QuerySwapChainSupport(VkPhysicalDevice currentDevice) {
        SwapChainSupportDetails details;
        if (surface == VK_NULL_HANDLE) { // Cannot query support without a surface
            VKENG_WARN("QuerySwapChainSupport called without a valid surface.");
            return details; // Return empty details
        }

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(currentDevice, surface, &details.capabilities);
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(currentDevice, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(currentDevice, surface, &formatCount, details.formats.data());
        }
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(currentDevice, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(currentDevice, surface, &presentModeCount, details.presentModes.data());
        }
        return details;
    }

    void VulkanContext::CreateLogicalDevice() {
        VKENG_INFO("Creating Logical Device...");
        QueueFamilyIndices indices = FindQueueFamilies(physicalDevice);
        if (!indices.graphicsFamily.has_value()) throw std::runtime_error("Graphics queue family not found for logical device!");
        // If surface is NULL (dummy context), presentFamily might not be set.
        // We assign it to graphicsFamily in FindQueueFamilies for dummy case.
        if (!indices.presentFamily.has_value()) throw std::runtime_error("Present queue family not found for logical device!");


        graphicsQueueFamily = indices.graphicsFamily.value();
        presentQueueFamily = indices.presentFamily.value();

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {graphicsQueueFamily, presentQueueFamily};

        float queuePriority = 1.0f;
        for (uint32_t queueFamilyIndex : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeaturesToEnable{}; // Intentionally default-initialize (all false)
        // Enable specific features based on checks in IsDeviceSuitable or direct needs
        if (physicalDeviceFeatures.samplerAnisotropy) {
             deviceFeaturesToEnable.samplerAnisotropy = VK_TRUE;
        }
        // if (physicalDeviceFeatures.fillModeNonSolid) { // For wireframe rendering
        //      deviceFeaturesToEnable.fillModeNonSolid = VK_TRUE;
        // }
        // Add more features as needed...


        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeaturesToEnable;

        // Prepare enabled extensions, including portability subset if needed
        std::vector<const char*> enabledDeviceExtensions = globalDeviceExtensions;
        #ifdef __APPLE__
        uint32_t extCount;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availExts(extCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, availExts.data());
        bool needsPortabilitySubset = false;
        for (const auto& ext : availExts) {
            if (strcmp(ext.extensionName, "VK_KHR_portability_subset") == 0) {
                needsPortabilitySubset = true;
                break;
            }
        }
        if (needsPortabilitySubset) {
            // Ensure it's not already in globalDeviceExtensions
            if (std::find_if(enabledDeviceExtensions.begin(), enabledDeviceExtensions.end(),
                             [](const char* extName){ return strcmp(extName, "VK_KHR_portability_subset") == 0; }) == enabledDeviceExtensions.end()) {
                enabledDeviceExtensions.push_back("VK_KHR_portability_subset");
            }
        }
        #endif

        createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledDeviceExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();
        VKENG_INFO("Enabled Device Extensions for Logical Device:");
        for(const char* extName : enabledDeviceExtensions) VKENG_INFO("  - {}", extName);


        if (enableValidationLayersGlobal) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(globalValidationLayers.size());
            createInfo.ppEnabledLayerNames = globalValidationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        // Chain Vulkan 1.1/1.2/1.3 features if needed
        // VkPhysicalDeviceVulkan13Features features13{};
        // features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        // features13.dynamicRendering = VK_TRUE;
        // features13.synchronization2 = VK_TRUE;
        // createInfo.pNext = &features13; // Chain this to createInfo
        // ... And so on for other feature structs, linking them with pNext

        VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));

        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);
        VKENG_INFO("Logical Device and Queues Created.");
    }

} // namespace VulkEng


















// 12 May 19:10 | -_-
// #include "VulkanContext.h"
// #include "VulkanUtils.h"
// #include "core/Log.h"
// #include <vector>
// #include <stdexcept>
// #include <set>
// #include <cstring> // For strcmp
// 
// #ifdef NDEBUG
// const bool enableValidationLayers = false;
// #else
// const bool enableValidationLayers = true;
// #endif
// 
// const std::vector<const char*> validationLayers = {
//     "VK_LAYER_KHRONOS_validation"
// };
// const std::vector<const char*> deviceExtensions = {
//     VK_KHR_SWAPCHAIN_EXTENSION_NAME
//     // Add VK_KHR_portability_subset if targeting MoltenVK on macOS/iOS and it's reported by device
// };
// 
// // --- Debug Callback ---
// VkResult CreateDebugUtilsMessengerEXT_local(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
//     auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
//     if (func != nullptr) return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
//     else return VK_ERROR_EXTENSION_NOT_PRESENT;
// }
// void DestroyDebugUtilsMessengerEXT_local(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
//     auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
//     if (func != nullptr) func(instance, debugMessenger, pAllocator);
// }
// static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback_local(
//     VkDebugUtilsMessageSeverityFlagBitsEXT ms, VkDebugUtilsMessageTypeFlagsEXT mt,
//     const VkDebugUtilsMessengerCallbackDataEXT* pCBD, void* pUD) {
//     if (ms >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
//         if (ms >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) VKENG_ERROR("[Vulkan Validation] {0}", pCBD->pMessage);
//         else if (ms >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) VKENG_WARN("[Vulkan Validation] {0}", pCBD->pMessage);
//     }
//     return VK_FALSE;
// }
// 
// namespace VulkEng {
//     VulkanContext::VulkanContext(Window& window, bool skipVulkanInit /*= false*/)
//         : m_Window(window), instance(VK_NULL_HANDLE), debugMessenger(VK_NULL_HANDLE),
//           surface(VK_NULL_HANDLE), physicalDevice(VK_NULL_HANDLE), device(VK_NULL_HANDLE),
//           graphicsQueue(VK_NULL_HANDLE), presentQueue(VK_NULL_HANDLE),
//           graphicsQueueFamily(UINT32_MAX), presentQueueFamily(UINT32_MAX),
//           mainRenderPass(VK_NULL_HANDLE), imageCount(0), minImageCount(0)
//     {
//         if (skipVulkanInit) {
//             VKENG_WARN("VulkanContext: Skipping Vulkan Initialization for Dummy/Null instance!");
//             return;
//         }
// 
//         VKENG_INFO("Initializing Vulkan Context...");
//         CreateInstance();
//         if (instance == VK_NULL_HANDLE) throw std::runtime_error("Vulkan instance creation failed in context.");
//         SetupDebugMessenger();
//         m_Window.CreateWindowSurface(instance, &surface);
//         if (surface == VK_NULL_HANDLE && m_Window.GetGLFWwindow() != nullptr) { // Only error if window was meant to be real
//             throw std::runtime_error("Vulkan surface creation failed.");
//         }
//         PickPhysicalDevice();
//         if (physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("No suitable physical device found.");
//         CreateLogicalDevice();
//         if (device == VK_NULL_HANDLE) throw std::runtime_error("Logical device creation failed.");
//         VKENG_INFO("Vulkan Context Initialized.");
//     }
// 
//     VulkanContext::~VulkanContext() {
//         VKENG_INFO("Destroying Vulkan Context...");
//         if (device != VK_NULL_HANDLE) vkDestroyDevice(device, nullptr);
//         if (surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance, surface, nullptr);
//         if (enableValidationLayers && debugMessenger != VK_NULL_HANDLE) DestroyDebugUtilsMessengerEXT_local(instance, debugMessenger, nullptr);
//         if (instance != VK_NULL_HANDLE) vkDestroyInstance(instance, nullptr);
//         VKENG_INFO("Vulkan Context Destroyed.");
//     }
// 
//     void VulkanContext::CreateInstance() {
//         if (enableValidationLayers && !CheckValidationLayerSupport()) {
//             throw std::runtime_error("Validation layers requested, but not available!");
//         }
//         VkApplicationInfo appInfo{}; /* ... set appInfo fields ... */
//         appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
//         appInfo.pApplicationName = "Vulkan Engine";
//         appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
//         appInfo.pEngineName = "VulkEng";
//         appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
//         appInfo.apiVersion = VK_API_VERSION_1_3;
// 
// 
//         VkInstanceCreateInfo createInfo{};
//         createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
//         createInfo.pApplicationInfo = &appInfo;
//         auto requiredExtensions = GetRequiredInstanceExtensions();
//         createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
//         createInfo.ppEnabledExtensionNames = requiredExtensions.data();
// 
//         VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
//         if (enableValidationLayers) {
//             createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
//             createInfo.ppEnabledLayerNames = validationLayers.data();
//             PopulateDebugMessengerCreateInfo(debugCreateInfo);
//             createInfo.pNext = &debugCreateInfo;
//         } else {
//             createInfo.enabledLayerCount = 0;
//             createInfo.pNext = nullptr;
//         }
// 
//         // For macOS with MoltenVK if portability extension is present
//         #ifdef __APPLE__
//         bool portabilityEnumeration = false;
//         for(const char* ext : requiredExtensions) {
//             if (strcmp(ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
//                 portabilityEnumeration = true;
//                 break;
//             }
//         }
//         if(portabilityEnumeration) {
//             createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
//         }
//         #endif
// 
// 
//         VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
//     }
// 
//     bool VulkanContext::CheckValidationLayerSupport() {
//         uint32_t layerCount;
//         vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
//         std::vector<VkLayerProperties> availableLayers(layerCount);
//         vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
//         for (const char* layerName : validationLayers) {
//             bool layerFound = false;
//             for (const auto& layerProperties : availableLayers) {
//                 if (strcmp(layerName, layerProperties.layerName) == 0) {
//                     layerFound = true;
//                     break;
//                 }
//             }
//             if (!layerFound) return false;
//         }
//         return true;
//     }
// 
//     std::vector<const char*> VulkanContext::GetRequiredInstanceExtensions() {
//         uint32_t glfwExtensionCount = 0;
//         const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
//         std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
//         if (enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
// 
//         #ifdef __APPLE__ // For MoltenVK portability
//             extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
//             extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME); // Often needed with portability
//         #endif
//         return extensions;
//     }
// 
//     void VulkanContext::SetupDebugMessenger() {
//         if (!enableValidationLayers) return;
//         VkDebugUtilsMessengerCreateInfoEXT createInfo;
//         PopulateDebugMessengerCreateInfo(createInfo);
//         VK_CHECK(CreateDebugUtilsMessengerEXT_local(instance, &createInfo, nullptr, &debugMessenger));
//     }
// 
//     void VulkanContext::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
//         createInfo = {};
//         createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
//         createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
//         // createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
//         // createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
//         createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
//         createInfo.pfnUserCallback = debugCallback_local;
//     }
// 
//     void VulkanContext::PickPhysicalDevice() {
//         uint32_t deviceCount = 0;
//         vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
//         if (deviceCount == 0) throw std::runtime_error("Failed to find GPUs with Vulkan support!");
//         std::vector<VkPhysicalDevice> devices(deviceCount);
//         vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
// 
//         for (const auto& dev : devices) {
//             if (IsDeviceSuitable(dev)) {
//                 physicalDevice = dev;
//                 vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
//                 vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);
//                 VKENG_INFO("Selected Physical Device: {}", physicalDeviceProperties.deviceName);
//                 return;
//             }
//         }
//         throw std::runtime_error("Failed to find a suitable GPU!");
//     }
// 
//     bool VulkanContext::IsDeviceSuitable(VkPhysicalDevice dev) {
//         QueueFamilyIndices indices = FindQueueFamilies(dev);
//         bool extensionsSupported = CheckDeviceExtensionSupport(dev);
//         bool swapChainAdequate = false;
//         if (extensionsSupported && surface != VK_NULL_HANDLE) { // Only check swapchain if surface exists
//             SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(dev);
//             swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
//         } else if (surface == VK_NULL_HANDLE && m_Window.GetGLFWwindow() == nullptr) {
//             // If this is a dummy window context, swapchain adequacy is not a blocker
//              swapChainAdequate = true;
//         }
// 
// 
//         VkPhysicalDeviceFeatures supportedFeatures;
//         vkGetPhysicalDeviceFeatures(dev, &supportedFeatures);
//         bool featuresSupported = supportedFeatures.samplerAnisotropy; // Add more required features here
// 
//         // Check for Blit support (moved here for early check)
//         VkFormatProperties formatProps;
//         vkGetPhysicalDeviceFormatProperties(dev, VK_FORMAT_R8G8B8A8_SRGB, &formatProps); // Common texture format
//         bool blitSupport = (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) &&
//                            (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) &&
//                            (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
//         if (!blitSupport) VKENG_WARN("Device format R8G8B8A8_SRGB lacks full blit support for mipmapping!");
// 
// 
//         return indices.IsComplete() && extensionsSupported && swapChainAdequate && featuresSupported && blitSupport;
//     }
// 
//     QueueFamilyIndices VulkanContext::FindQueueFamilies(VkPhysicalDevice dev) {
//         QueueFamilyIndices indices;
//         uint32_t queueFamilyCount = 0;
//         vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
//         std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
//         vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());
//         int i = 0;
//         for (const auto& queueFamily : queueFamilies) {
//             if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphicsFamily = i;
//             if (surface != VK_NULL_HANDLE) { // Only check present support if surface exists
//                  VkBool32 presentSupport = false;
//                  vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
//                  if (presentSupport) indices.presentFamily = i;
//             } else if (m_Window.GetGLFWwindow() == nullptr) { // Dummy window context
//                 indices.presentFamily = i; // Assume present support for dummy
//             }
// 
//             if (indices.IsComplete()) break;
//             i++;
//         }
//         return indices;
//     }
// 
//     bool VulkanContext::CheckDeviceExtensionSupport(VkPhysicalDevice dev) {
//         uint32_t extensionCount;
//         vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, nullptr);
//         std::vector<VkExtensionProperties> availableExtensions(extensionCount);
//         vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, availableExtensions.data());
//         std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
// 
//         // For macOS MoltenVK, if VK_KHR_portability_subset is reported by physical device, it's required.
//         #ifdef __APPLE__
//         bool needsPortabilitySubset = false;
//         for (const auto& ext : availableExtensions) {
//             if (strcmp(ext.extensionName, "VK_KHR_portability_subset") == 0) {
//                 needsPortabilitySubset = true;
//                 break;
//             }
//         }
//         if (needsPortabilitySubset) {
//             required.insert("VK_KHR_portability_subset");
//         }
//         #endif
// 
//         for (const auto& extension : availableExtensions) required.erase(extension.extensionName);
//         return required.empty();
//     }
// 
//     SwapChainSupportDetails VulkanContext::QuerySwapChainSupport(VkPhysicalDevice dev) {
//         SwapChainSupportDetails details;
//         if (surface == VK_NULL_HANDLE) return details; // Cannot query without a surface
// 
//         vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &details.capabilities);
//         uint32_t formatCount;
//         vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, nullptr);
//         if (formatCount != 0) {
//             details.formats.resize(formatCount);
//             vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, details.formats.data());
//         }
//         uint32_t presentModeCount;
//         vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &presentModeCount, nullptr);
//         if (presentModeCount != 0) {
//             details.presentModes.resize(presentModeCount);
//             vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &presentModeCount, details.presentModes.data());
//         }
//         return details;
//     }
// 
//     void VulkanContext::CreateLogicalDevice() {
//         QueueFamilyIndices indices = FindQueueFamilies(physicalDevice);
//         if (!indices.graphicsFamily.has_value()) throw std::runtime_error("Graphics queue family not found!");
//         // For dummy context, presentFamily might not be found if surface is null, handle gracefully or ensure it's set
//         if (!indices.presentFamily.has_value() && surface != VK_NULL_HANDLE) {
//             throw std::runtime_error("Present queue family not found!");
//         }
// 
// 
//         graphicsQueueFamily = indices.graphicsFamily.value();
//         presentQueueFamily = indices.presentFamily.has_value() ? indices.presentFamily.value() : graphicsQueueFamily; // Fallback for dummy
// 
// 
//         std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
//         std::set<uint32_t> uniqueQueueFamilies = {graphicsQueueFamily, presentQueueFamily};
//         float queuePriority = 1.0f;
//         for (uint32_t queueFamily : uniqueQueueFamilies) {
//             VkDeviceQueueCreateInfo queueCreateInfo{};
//             queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
//             queueCreateInfo.queueFamilyIndex = queueFamily;
//             queueCreateInfo.queueCount = 1;
//             queueCreateInfo.pQueuePriorities = &queuePriority;
//             queueCreateInfos.push_back(queueCreateInfo);
//         }
// 
//         VkPhysicalDeviceFeatures deviceFeatures{}; // Enable features used
//         deviceFeatures.samplerAnisotropy = VK_TRUE;
//         // deviceFeatures.fillModeNonSolid = VK_TRUE; // For wireframe rendering if needed
// 
//         VkDeviceCreateInfo createInfo{};
//         createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
//         createInfo.pQueueCreateInfos = queueCreateInfos.data();
//         createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
//         createInfo.pEnabledFeatures = &deviceFeatures;
// 
//         // Check for portability subset extension again for device creation
//         std::vector<const char*> actualDeviceExtensions = deviceExtensions;
//         #ifdef __APPLE__
//         uint32_t extCount;
//         vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
//         std::vector<VkExtensionProperties> availExts(extCount);
//         vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, availExts.data());
//         bool needsPortabilitySubset = false;
//         for (const auto& ext : availExts) {
//             if (strcmp(ext.extensionName, "VK_KHR_portability_subset") == 0) {
//                 needsPortabilitySubset = true;
//                 break;
//             }
//         }
//         if (needsPortabilitySubset) {
//             bool found = false;
//             for(const char* requiredExt : actualDeviceExtensions) {
//                 if(strcmp(requiredExt, "VK_KHR_portability_subset") == 0) {
//                     found = true; break;
//                 }
//             }
//             if(!found) actualDeviceExtensions.push_back("VK_KHR_portability_subset");
//         }
//         #endif
// 
//         createInfo.enabledExtensionCount = static_cast<uint32_t>(actualDeviceExtensions.size());
//         createInfo.ppEnabledExtensionNames = actualDeviceExtensions.data();
// 
// 
//         if (enableValidationLayers) {
//             createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
//             createInfo.ppEnabledLayerNames = validationLayers.data();
//         } else {
//             createInfo.enabledLayerCount = 0;
//         }
//         VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));
//         vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
//         vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);
//     }
// 
//     // --- Single Time Command Helpers Implementation (Moved from VulkanUtils for context access) ---
//     VkCommandBuffer VulkanContext::BeginSingleTimeCommandsHelper(VkCommandPool pool) {
//         if (device == VK_NULL_HANDLE || pool == VK_NULL_HANDLE) {
//             VKENG_ERROR("BeginSingleTimeCommandsHelper: Invalid device or command pool.");
//             return VK_NULL_HANDLE;
//         }
//         VkCommandBufferAllocateInfo allocInfo{};
//         allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
//         allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
//         allocInfo.commandPool = pool;
//         allocInfo.commandBufferCount = 1;
//         VkCommandBuffer commandBuffer;
//         VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));
//         VkCommandBufferBeginInfo beginInfo{};
//         beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
//         beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
//         VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
//         return commandBuffer;
//     }
// 
//     void VulkanContext::EndSingleTimeCommandsHelper(VkCommandPool pool, VkCommandBuffer commandBuffer) {
//          if (device == VK_NULL_HANDLE || pool == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE || graphicsQueue == VK_NULL_HANDLE) {
//             VKENG_ERROR("EndSingleTimeCommandsHelper: Invalid device, pool, command buffer, or graphics queue.");
//             if(commandBuffer != VK_NULL_HANDLE && pool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { // Attempt to free if possible
//                 vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
//             }
//             return;
//         }
//         VK_CHECK(vkEndCommandBuffer(commandBuffer));
//         VkSubmitInfo submitInfo{};
//         submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//         submitInfo.commandBufferCount = 1;
//         submitInfo.pCommandBuffers = &commandBuffer;
//         VkFence fence;
//         VkFenceCreateInfo fenceInfo{};
//         fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
//         VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fence));
//         VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence));
//         VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
//         vkDestroyFence(device, fence, nullptr);
//         vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
//     }
// 
// } // namespace VulkEng

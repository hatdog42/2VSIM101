#ifndef UTILITIES_H
#define UTILITIES_H

#include "vulkan/vulkan_core.h"
#include <optional>
#include <fstream>
#include <QDebug>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

const int MAX_FRAMES_IN_FLIGHT{ 2 };
const int MAX_TEXTURES{ 1000 };          //Max Textures

// using inline so header-only definition doesn't produce duplicate symbols when included from multiple translation units (TUs).
inline const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
inline const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NDEBUG
inline bool enableValidationLayers = false;
#else
inline bool enableValidationLayers = true;
#endif

// Make these inline to avoid multiple-definition linker errors when included from multiple TUs.
inline VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

inline void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
        #ifdef _WIN32
        return graphicsFamily.has_value() && presentFamily.has_value();

        #elif defined(__APPLE__)
        //Mac-hack - we only care about the graphicsFamily, since that includes the presentFamily
        return graphicsFamily.has_value(); // && presentFamily.has_value();
        #endif
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

static std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
    else
        qDebug() << "Opened file " << filename;

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

// Uniform Buffer update
// Using vec4s because of allignment
struct alignas(16) UniformBufferObject
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 cameraPosition;
    glm::vec4 lightPosition{glm::vec4(3.0, 10.0, 6.0, 0.f)};
    glm::vec4 lightColor{glm::vec4(1.0, 0.9, 0.5, 0.f)};        // Yellow
    glm::vec4 lightParams{glm::vec4(0.1f, 0.5f, 0.f, 0.f)};     // ambient, diffuse strength, 0, 0
};

// PushConstants for Lighting
// Using vec4s because of allignment
struct alignas(16) PushConstants
{
    glm::mat4 model{glm::mat4(1.0f)};
    glm::vec4 objectColor{glm::vec4(1.0, 1.0, 1.0, 0.f)};
    glm::vec4 specularParams{glm::vec4(1.0f, 256.0f, 0.f, 0.f)};      //strenght, exponent, 0, 0
};

// Sets correct PATH for where to look for shaders and models, depending on the build environment
// This is necessary since the working directory is different for Visual Studio and Qt Creator
// Build system test is done in CMake
#if defined(BUILD_ENV_VISUAL_STUDIO)
inline const std::string PATH = "../../../";
#elif defined(BUILD_ENV_QTCREATOR)
inline const std::string PATH = "../../";
#else
inline const std::string PATH = "../../"; // fallback e.g. Mac
#endif

#endif // UTILITIES_H

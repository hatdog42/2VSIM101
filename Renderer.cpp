#include "Renderer.h"
#include "Camera.h"
#include "Collider.h"
#include "GameObject.h"
#include "Mesh.h"
#include "PointCloud.h"
#include "Utilities.h"

#define STB_IMAGE_IMPLEMENTATION     // Needs to be done ONCE before stb_image is used!
#include "External/stb_image.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include "Logger.h"
#include "MainWindow.h"
#include "Texture.h"
#include "Light.h"
#include "Engine.h"
#include "Terrain.h"
#include "Transform.h"

#ifdef _WIN32			// Windows specific includes
#define NOMINMAX		// Prevent Windows.h from defining min/max macros
#include <windows.h>	// For HWND and GetModuleHandle
#include <vulkan/vulkan_win32.h>

#elif defined(__APPLE__)
#include <vulkan/vulkan_macos.h>
#include <vulkan/vulkan_metal.h>
#include <objc/objc.h>
#endif

Renderer::Renderer(QWindow* parent) : QWindow(parent), mLogger(Logger::getInstance())
{
    setSurfaceType(QWindow::VulkanSurface);		// Seems to be important to get this to work

    //Setup is done in the initVulkan() function
    //size of this window has to be set before initVulkan() is called
    //this is done in the MainWindow() constructor

    mEngine = Engine::getInstance();
    mEngine->setRenderer(this);
}

Renderer::~Renderer()
{
    LOG("VulkanRenderer Destructor");
    cleanup();
}

// Called from Engine
void Renderer::update()
{
    drawFrame();        // Draws one frame
    requestUpdate();    // Tells Qt that we want to update again, which calls event() which calls Engine->update()
}

void Renderer::initVulkan()
{
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createColorResources();
    createDepthResources();
    createFramebuffers();
    // createVertexBuffer(mMeshes); must be done after we have loaded meshes
    createUniformBuffers();
    createDescriptorPool();

    // Texture update: - textures must be made before the Descriptor sets!
    // This should be made more smooth, and probably also be done from
    // the engine class
    createTextureSampler();     // Same sampler used for all textures;
    mEngine->loadTextures();     // Load textures into the engine, and create the Vulkan image and image view for each texture

    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();

    // We make renderable data after Vulkan is set up.
    mEngine->loadMeshes();
    mEngine->loadPointCloud();

    // Making the Game Objects of the scene
    mEngine->loadScene();

    mCamera = new Camera();
    // mLight = new Light(); not used yet, variables hard coded in ubo/pushconstants
}

void Renderer::cleanupSwapChain()
{

    // Destroy framebuffers first (they reference image views)
    for (auto framebuffer : swapChainFramebuffers)
    {
        if (framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
    }
    swapChainFramebuffers.clear();

    // Destroy the per-swapchain image views
    for (auto imageView : swapChainImageViews)
    {
        if (imageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, imageView, nullptr);
        }
    }
    swapChainImageViews.clear();

    // Destroy multisample color resources (if present)
    if (colorImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, colorImageView, nullptr);
        colorImageView = VK_NULL_HANDLE;
    }
    if (colorImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, colorImage, nullptr);
        colorImage = VK_NULL_HANDLE;
    }
    if (colorImageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, colorImageMemory, nullptr);
        colorImageMemory = VK_NULL_HANDLE;
    }

    // Destroy depth resources (must be removed when recreating swapchain)
    if (depthImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, depthImageView, nullptr);
        depthImageView = VK_NULL_HANDLE;
    }
    if (depthImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, depthImage, nullptr);
        depthImage = VK_NULL_HANDLE;
    }
    if (depthImageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, depthImageMemory, nullptr);
        depthImageMemory = VK_NULL_HANDLE;
    }

    // Finally destroy the swapchain itself
    if (swapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        swapChain = VK_NULL_HANDLE;
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);
}

void Renderer::cleanup()
{
    // Ensure all GPU work has finished before destroying resources
    if (device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(device);

    // Destroy swapchain and related per-swapchain resources
    cleanupSwapChain();

    // Free the point cloud's GPU resources and CPU data.
    if (pointCloudVertexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, pointCloudVertexBuffer, nullptr);
        pointCloudVertexBuffer = VK_NULL_HANDLE;
    }
    if (pointCloudVertexBufferMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, pointCloudVertexBufferMemory, nullptr);
        pointCloudVertexBufferMemory = VK_NULL_HANDLE;
    }
    delete mEngine->mPointCloud;
    mEngine->mPointCloud = nullptr;

    // Depthbuffer:
    if (depthImageView != VK_NULL_HANDLE) 
    {
        vkDestroyImageView(device, depthImageView, nullptr); 
        depthImageView = VK_NULL_HANDLE; 
    }
    if (depthImage != VK_NULL_HANDLE) 
    {
        vkDestroyImage(device, depthImage, nullptr);
        depthImage = VK_NULL_HANDLE; 
    }
    if (depthImageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, depthImageMemory, nullptr);
        depthImageMemory = VK_NULL_HANDLE;
    }

    // Texture Sampler
    if (mTextureSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device, mTextureSampler, nullptr);
        mTextureSampler = VK_NULL_HANDLE;
    }

    // Texture Descriptors:
    if (samplerDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device, samplerDescriptorPool, nullptr);
        samplerDescriptorPool = VK_NULL_HANDLE;
    }
    if (samplerSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, samplerSetLayout, nullptr);
        samplerSetLayout = VK_NULL_HANDLE;
    }

    // UBO descriptors:
    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
    {
        if (uniformBuffers[i] != VK_NULL_HANDLE) 
        {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            uniformBuffers[i] = VK_NULL_HANDLE; 
        }
        if (uniformBuffersMemory[i] != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
            uniformBuffersMemory[i] = VK_NULL_HANDLE;
        }
    }

    for (auto mesh : mEngine->mMeshes)
    {
        // destroy buffers for each mesh - done inside the Mesh class destructor
        delete mesh;
    }
    mEngine->mMeshes.clear();

    // Pipeline and layout
    if (graphicsPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        graphicsPipeline = VK_NULL_HANDLE;
    }
    if (graphicsPipeline2 != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, graphicsPipeline2, nullptr);
        graphicsPipeline2 = VK_NULL_HANDLE;
    }
    if (pointCloudPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pointCloudPipeline, nullptr);
        pointCloudPipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    // Render pass
    if (renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }

    // Destroy semaphores and fences using actual vector sizes
    for (auto& sem : renderFinishedSemaphores)
    {
        if (sem != VK_NULL_HANDLE)
            vkDestroySemaphore(device, sem, nullptr);
    }
    renderFinishedSemaphores.clear();

    for (auto& sem : imageAvailableSemaphores)
    {
        if (sem != VK_NULL_HANDLE)
            vkDestroySemaphore(device, sem, nullptr);
    }
    imageAvailableSemaphores.clear();

    for (auto& f : inFlightFences)
    {
        if (f != VK_NULL_HANDLE)
            vkDestroyFence(device, f, nullptr);
    }
    inFlightFences.clear();

    // Command pool
    if (commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(device, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }

    // Texture update - Delete the textures created - variables in the Texture class
    for(auto texture : mEngine->mTextures)
    {
        if (texture->mTextureImageView != VK_NULL_HANDLE)
            vkDestroyImageView(device, texture->mTextureImageView, nullptr);
        if (texture->mTextureImage != VK_NULL_HANDLE)
            vkDestroyImage(device, texture->mTextureImage, nullptr);
        if (texture->mTextureImageMemory != VK_NULL_HANDLE)
            vkFreeMemory(device, texture->mTextureImageMemory, nullptr);
        delete texture;
    }
    mEngine->mTextures.clear();
    samplerDescriptorSets.clear();

    // Finally destroy the logical device
    if (device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }

    // Debug messenger, surface and instance
    if (enableValidationLayers && debugMessenger) 
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    if (surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
    if (instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
}

void Renderer::recreateSwapChain() 
{
    LOGH("Recreating SwapChain");
    int width = 0, height = 0;
    // glfwGetFramebufferSize(window, &width, &height);
    // while (width == 0 || height == 0) {
        // glfwGetFramebufferSize(window, &width, &height);
        // glfwWaitEvents();
    // }

    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createColorResources();     // Multisample update
    createDepthResources();
    createFramebuffers();
}

void Renderer::createInstance()
{
#ifdef _WIN32
    if (enableValidationLayers && !checkValidationLayerSupport())
        throw std::runtime_error("validation layers requested, but not available!");

#elif defined(__APPLE__)    //Validation layers on Mac not working atm
    enableValidationLayers = false;
    LOG("Mac has no support for Validation layers atm.");
#endif

    //Information about the application itself
    //Mostly used for debugging
    //Needed in VkInstanceCreateInfo
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Not using GLFW, so need to do this another way
    std::vector<const char*> extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        throw std::runtime_error("failed to create instance!");
    else
        LOG("createInstance successful");
}

void Renderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void Renderer::setupDebugMessenger()
{
    if (!enableValidationLayers)
        return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
        throw std::runtime_error("failed to set up debug messenger!");
    else
        LOG("setupDebugMessenger successful");
}

void Renderer::createSurface()
{
    //Qt version, since we are not using GLFW:
#ifdef Q_OS_WIN
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = reinterpret_cast<HWND>(this->winId());
    createInfo.hinstance = GetModuleHandle(nullptr);

    if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create a surface!");
    else
        LOG("Successfully created a surface!");

#elif defined(__APPLE__)
    void* nsview = reinterpret_cast<void*>(this->winId());

    extern void* setupMetalLayerForView(void* nsview);
    CAMetalLayer* metalLayer =
        (CAMetalLayer*)setupMetalLayerForView(nsview);

    // setupMetalLayerForView(nsview);

    VkMetalSurfaceCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    // createInfo.pNext = NULL;
    // createInfo.flags = 0;
    createInfo.pLayer = metalLayer; //nsview;

    //std::this_thread::sleep_for(std::chrono::milliseconds(500));  // Small delay to let Metal settle

    // This must happen AFTER the layer is successfully set up
    if (vkCreateMetalSurfaceEXT(instance, &createInfo, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create a surface!");
    else
        LOG("Successfully created a surface!");
#endif
}

void Renderer::pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0)
        throw std::runtime_error("failed to find GPUs with Vulkan support!");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // These lines are a little extended to print out device info for all Vulkan GPUs:
    // Print the names of the physical devices and check if they has required extensions
    int i{ 0 };
    for (const auto& device : devices)
    {
        LOGH("Finding physical devices:");
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        LOGP("Physical Device No.%d, Name: %s", i, deviceProperties.deviceName);
        LOGP("   - Max Vulkan API support: %u.%u.%u",
               VK_VERSION_MAJOR(deviceProperties.apiVersion),
               VK_VERSION_MINOR(deviceProperties.apiVersion),
               VK_VERSION_PATCH(deviceProperties.apiVersion));

        if (isDeviceSuitable(device))
            LOGP("   - %s is a suitable device!", deviceProperties.deviceName);

        else
            LOGP("   - %s is not suitable device", deviceProperties.deviceName);

        i++;
    }

    // This will automatically select the first device that is suitable:
    // A little extended to print out GPU used
    for (const auto& device : devices)
    {
        if (isDeviceSuitable(device))
        {
            physicalDevice = device;
            msaaSamples = getMaxUsableSampleCount();
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            LOGW("Selected physical device:");
            LOGP("   - %s", deviceProperties.deviceName);
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("failed to find a suitable GPU!");
    else
        LOG("Found suitable GPU");
}

void Renderer::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    // This is tested as supportedFeatures in isDeviceSuitable()
    // called in pickPhysicalDevice() to see if
    // samplerAnisotropy is supported on this GPU
    deviceFeatures.samplerAnisotropy = VK_TRUE;     // Texture update
    deviceFeatures.fillModeNonSolid = VK_TRUE;      // <-- Enable non-solid fill (lines, points)

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error("failed to create logical device!");  
    else
       LOG("Created logical device");

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void Renderer::createSwapChain()
{
    //Get swap chain details so we can pick best settings
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    //1. Choose best surface format
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    //2. Choose best presentation mode
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    //3. Choose swapchain image resolution
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    //How many images are in the swap chain? Recommended to request at least 1 more than the minimum
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    //Check if image count is greater than the max allowed, clamp to max allowed
    //0 is a special value for maxImageCount that means there is no maximum
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        imageCount = swapChainSupport.capabilities.maxImageCount;

    //Creation information for the swap chain
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    //Get queue family indices
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
        throw std::runtime_error("failed to create swap chain!");
    else
        LOG("Created swap chain");

    //Get swap chain images (first count, then values)
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    LOGP(" - swapChainImageCount %d:", imageCount);

    //Store for later use
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;

    // Track per-image fence usage so we don't reuse semaphores while an image is still in use
    imagesInFlight.assign(swapChainImages.size(), VK_NULL_HANDLE);
}

void Renderer::createImageViews()
{
    swapChainImageViews.resize(swapChainImages.size());

    for (uint32_t i = 0; i < swapChainImages.size(); i++)
    {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat,
                                                 VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
    // for (size_t i = 0; i < swapChainImages.size(); i++)
    // {
    //     VkImageViewCreateInfo createInfo{};
    //     createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    //     createInfo.image = swapChainImages[i];
    //     createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    //     createInfo.format = swapChainImageFormat;
    //     createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    //     createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    //     createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    //     createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    //     createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //     createInfo.subresourceRange.baseMipLevel = 0;
    //     createInfo.subresourceRange.levelCount = 1;
    //     createInfo.subresourceRange.baseArrayLayer = 0;
    //     createInfo.subresourceRange.layerCount = 1;

    //     if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
    //         throw std::runtime_error("failed to create image views!");
    //     else
    //         LOG("Created image views!");
    // }
}

void Renderer::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.samples = msaaSamples;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Multisample update

    // Depth buffer update:
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = msaaSamples;      // Multisample update
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;               // clear depth buffer before use
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Multisample update
    // Multisampled images cannot be presented directly.
    // We first need to resolve them to a regular image
    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = swapChainImageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth Attachment
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Multisampling Attachment
    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;      // Depth buffer update
    subpass.pResolveAttachments = &colorAttachmentResolveRef;   // Multisample update

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; //Multisample update
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Depth buffer - we now have two attachments
    // Multisampling - we now have three attachments
    // The order has to be the same as .attachment = given above
    std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
        throw std::runtime_error("failed to create render pass!");
    else
        LOG("created render pass!");
}

void Renderer::createDescriptorSetLayout()
{
    // Uniform Descriptor Set Layout
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;                    // Set 0, Binding 0
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

    std::array<VkDescriptorSetLayoutBinding, 1> bindings = {uboLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());           // Texture update - see below
    layoutInfo.pBindings = bindings.data();                                     // Texture update - see below

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor set layout!");
    else
        LOG("created descriptor set layout");

    // Texture Sampler Descriptor Set Layout
    // Descriptor for Textures:
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;                   // Set 1, Binding 0
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
    textureLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    textureLayoutInfo.bindingCount = 1;    // Using the same sampler for all textures
    textureLayoutInfo.pBindings = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &textureLayoutInfo, nullptr, &samplerSetLayout) != VK_SUCCESS)
        throw std::runtime_error("failed to create Sampler set layout!");
    else
        LOG("created Sampler set layout");
}

void Renderer::createGraphicsPipeline()
{
    const std::string shaderPath = std::string(PROJECT_SOURCE_PATH) + "/Shaders/";
    auto vertShaderCode = readFile(shaderPath + "PhongVert.spv");
    auto fragShaderCode = readFile(shaderPath + "PhongFrag.spv");
    auto pointVertShaderCode = readFile(shaderPath + "vert.spv");
    auto pointFragShaderCode = readFile(shaderPath + "frag.spv");

    LOG("Vertex shader:");
    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    LOGH("Fragment shader:");
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
    VkShaderModule pointVertShaderModule = createShaderModule(pointVertShaderCode);
    VkShaderModule pointFragShaderModule = createShaderModule(pointFragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex buffer update:
    // Define how the Vertices are layed out
    auto bindingDescription = Vertex::getBindingDescription();      // auto == VkVertexInputBindingDescription
    auto attributeDescriptions = Vertex::getAttributeDescriptions();    // auto == std::array<VkVertexInputAttributeDescription, 2>

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    // Vertex buffer update end

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;  // VK_POLYGON_MODE_FILL, VK_POLYGON_MODE_LINE, VK_POLYGON_MODE_POINT
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;        // VK_CULL_MODE_NONE, VK_CULL_MODE_BACK_BIT
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // VK_FRONT_FACE_CLOCKWISE, VK_FRONT_FACE_COUNTER_CLOCKWISE
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = msaaSamples;   // Multisample update

    // Depth buffer update
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;             // turn on depth testing
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // PushConstant update
    // PushConstants are a small amount of data that can be passed to shaders
    // - max size guaranteed is 128 bytes
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    // Update to support several texture - we now have two LayoutSets
    std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = {descriptorSetLayout, samplerSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();   //Multiple Textures update
    pipelineLayoutInfo.pushConstantRangeCount = 1;                  //pushConstant update
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;    //pushConstant update

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout!");
    else
        LOG("created pipeline layout!");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &depthStencil;        // Depth buffer update
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
        throw std::runtime_error("failed to create graphics pipeline!");
    else
        LOG("created graphics pipeline!");

    rasterizer.polygonMode = VK_POLYGON_MODE_LINE;  // VK_POLYGON_MODE_FILL, VK_POLYGON_MODE_LINE, VK_POLYGON_MODE_POINT
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline2) != VK_SUCCESS)
        throw std::runtime_error("failed to create graphics pipeline!");
    else
        LOG("created graphics pipeline2!");
    
    // Create a pipeline that draws each vertex as one point.
    shaderStages[0].module = pointVertShaderModule;
    shaderStages[1].module = pointFragShaderModule;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pointCloudPipeline) != VK_SUCCESS)
        throw std::runtime_error("failed to create point-cloud pipeline!");
    else
        LOG("created point-cloud pipeline!");

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, pointFragShaderModule, nullptr);
    vkDestroyShaderModule(device, pointVertShaderModule, nullptr);
}

void Renderer::createFramebuffers()
{
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++)
    {
        // Depth buffer update
        std::array<VkImageView, 3> attachments = {
            colorImageView,          // Multisample update
            depthImageView,          // only need one, it will be reused while a colorImage is presented
            swapChainImageViews[i]   // Multisample update
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create framebuffer!");
        else
            LOG("created framebuffer!");
    }
}

void Renderer::createCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create command pool!");
    else
        LOG("created command pool!");
}

// Texture update:
// We are holding the actual texture data in the Texture class
void Renderer::createTextureImage(Texture *textureIn)
{
    // Using the variables in the Texture class
    // int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(std::string(PATH + "Assets/" + textureIn->mTextureName).c_str(),
                                &textureIn->mTexWidth, &textureIn->mTexHeight, &textureIn->mTexChannels, STBI_rgb_alpha);
    // VkDeviceSize imageSize = textureIn->mTexWidth * textureIn->mTexHeight * 4;

    // Prepare a small fallback buffer (4x4 RGBA pink) if loading fails.
    // Keep it in a local vector so its memory remains valid until we upload it.
    std::vector<stbi_uc> fallback;
    bool stbiAllocated{ true }; // true if 'pixels' was returned by stbi_load and must be freed with stbi_image_free

    //If texture is NOT found:
    if (!pixels)
    {
        // Added fallback to a 4x4 pink texture if loading fails, so we can see something instead of crashing the program
        LOGE("ERROR: Could not load texture!");
        LOGP("    Could not open file for reading: %s", textureIn->mTextureName.c_str());
        LOGW("    Using 4x4 pink fallback texture instead");

        textureIn->mTexWidth = 4;
        textureIn->mTexHeight = 4;
        textureIn->mTexChannels = 4; // RGBA

        fallback.resize(64);    //  == mTexWidth * mTexHeight * mTexChannels

        // Fill with opaque magenta/pink (R=255, G=0, B=255, A=255)
        for (size_t i = 0; i < fallback.size(); i += 4)
        {
            fallback[i + 0] = 255; // R
            fallback[i + 1] = 0; // G
            fallback[i + 2] = 255; // B
            fallback[i + 3] = 255; // A
        }

        pixels = fallback.data(); // point to fallback data for the upload
        stbiAllocated = false;    // don't call stbi_image_free on fallback.data

        textureIn->mTextureName = "Fallback Pink Texture";
    }
    else
        LOGP("loaded texture image %s ", textureIn->mTextureName.c_str());

    // MipMap update
    textureIn->mMipLevels = static_cast<uint32_t>(
                                std::floor(std::log2(
                                std::max(textureIn->mTexWidth, textureIn->mTexHeight)))
                                ) + 1;

    VkDeviceSize imageSize = textureIn->mTexWidth * textureIn->mTexHeight * 4;

    // temporary staging buffers:
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    // free stbi image only if it was allocated by stbi_load
    if (stbiAllocated)
        stbi_image_free(pixels);

    createImage(textureIn->mTexWidth, textureIn->mTexHeight, textureIn->mMipLevels, VK_SAMPLE_COUNT_1_BIT,
                VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureIn->mTextureImage, textureIn->mTextureImageMemory);

    transitionImageLayout(textureIn->mTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, textureIn->mMipLevels);

    copyBufferToImage(stagingBuffer, textureIn->mTextureImage, textureIn->mTexWidth, textureIn->mTexHeight);

    // MipMap update:
    //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps:
    generateMipmaps(textureIn->mTextureImage, VK_FORMAT_R8G8B8A8_SRGB, textureIn->mTexWidth,
                    textureIn->mTexHeight, textureIn->mMipLevels);

    // StagingBuffers no longer needed:
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Renderer::createTextureImageView(Texture *textureIn)
{
    textureIn->mTextureImageView = createImageView(textureIn->mTextureImage, VK_FORMAT_R8G8B8A8_SRGB,
                                                   VK_IMAGE_ASPECT_COLOR_BIT, textureIn->mMipLevels);
}

void Renderer::createTextureSampler() //Texture *textureIn)
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;     //enabled in createLogicalDevice()
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;                  //static_cast<float>(textureIn->mMipLevels/2);
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;     //static_cast<float>(textureIn->mMipLevels);
    samplerInfo.mipLodBias = 0.0f;  // Optional

    if (vkCreateSampler(device, &samplerInfo, nullptr, &mTextureSampler) != VK_SUCCESS)  //&textureIn->mTextureSampler
        throw std::runtime_error("failed to create texture sampler!");
    else
        LOG("created texture sampler!");
}

VkImageView Renderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;  // Multisample update
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;   //MipMap update
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
        throw std::runtime_error("failed to create texture image view!");
    else
        LOG("created texture image view");

    return imageView;
}

void Renderer::createColorResources()
{
    VkFormat colorFormat = swapChainImageFormat;

    createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples,
                colorFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImage, colorImageMemory);
    colorImageView = createImageView(colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

void Renderer::createDepthResources()
{
    // Get supported format for depth buffer
    VkFormat depthFormat = findDepthFormat();

    // Create Depth Buffer Image
    createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                depthImage, depthImageMemory);

    // Create Depth Buffer Image View
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
}

VkFormat Renderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            return format;
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            return format;
    }

    throw std::runtime_error("failed to find supported format!");
}

VkFormat Renderer::findDepthFormat()
{
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
}

void Renderer::createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples,
                           VkFormat format, VkImageTiling tiling,VkImageUsageFlags usage,
                           VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
    // Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;    //MipMap update
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;      // VK_FORMAT_R8G8B8A8_SRGB for textures
    imageInfo.tiling = tiling;      // VK_IMAGE_TILING_OPTIMAL for textures
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;        // VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT for textures
    imageInfo.samples = numSamples;     // VK_SAMPLE_COUNT_1_BIT; Multisample update
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("failed to create image!");
    else
        LOG("created image!");

    //Create memory for image:

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate image memory!");
    else
        LOG("allocated image memory!");

    // Connect memory to image
    vkBindImageMemory(device, image, imageMemory, 0);
}

// not in use yet
bool Renderer::hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void Renderer::createCommandBuffers()
{
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate command buffers!");
    else
        LOG("allocated command buffers!");
}

void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("failed to begin recording command buffer!");

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    // Depth buffer update
    std::array<VkClearValue, 2> clearValues{};
    // attachment 0 == color, 1 == depth, because we used that in createRenderPass()
    clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Moved Viewport and Scissor to before binding the pipeline
        // because these can be reused even if we bind a different pipeline
        // between different objects
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        if(mRenderLines)
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline2);
        else
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        // Vertex buffer from mMeshes vector
        for (GameObject* GOB : mEngine->mGameObjects)
        {
            VkBuffer vertexBuffers[] = { mEngine->mMeshes[GOB->mMesh]->vertexBuffer() };
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            // DescriptorSet update:
            // We now have one descriptor set for the ubo and one for the texture:
            std::array<VkDescriptorSet, 2> descriptorSetGroup = {descriptorSets[currentFrame],
                                                                 samplerDescriptorSets[GOB->mTexture]};

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
                                    static_cast<int32_t>(descriptorSetGroup.size()),
                                    descriptorSetGroup.data(),
                                    0, nullptr);


            mPushConstants.model = GOB->mTransform->TransformMatrix();

            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &mPushConstants);


            if(mEngine->mMeshes[GOB->mMesh]->hasIndices())
            {
                vkCmdBindIndexBuffer(commandBuffer, mEngine->mMeshes[GOB->mMesh]->indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mEngine->mMeshes[GOB->mMesh]->getIndexCount()), 1, 0, 0, 0);
            }
            else
                vkCmdDraw(commandBuffer, static_cast<uint32_t>(mEngine->mMeshes[GOB->mMesh]->getVertexCount()), 1, 0, 0);
        }

        // Draw the point cloud when its CPU and GPU data are ready.
        if (mEngine->mPointCloud != nullptr && pointCloudVertexBuffer != VK_NULL_HANDLE)
        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pointCloudPipeline);

            VkBuffer vertexBuffers[] = {pointCloudVertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            // The point shaders only need the camera descriptor.
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
                                    1, &descriptorSets[currentFrame], 0, nullptr);

            // Draw one vertex for each point.
            vkCmdDraw(commandBuffer,
                      static_cast<uint32_t>(mEngine->mPointCloud->getVertices().size()),
                      1, 0, 0);
        }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        throw std::runtime_error("failed to record command buffer!");
    // else
        // LOG("recorded command buffer!"); //spams the log
}

void Renderer::createSyncObjects()
{
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    // One render-finished semaphore per swapchain image to avoid swapchain-semaphore reuse issues
    renderFinishedSemaphores.resize(swapChainImages.size());

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    // Create per-frame image-available semaphores and fences
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        else
            LOGP("created synchronization objects for frame %d", i);
    }

    // Create per-swapchain-image render-finished semaphores
    for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i)
    {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create render-finished semaphore!");
    }
}

// Updated to create vertex buffers for all meshes in the mMeshes vector
void Renderer::createVertexBuffer(Mesh* mesh)
{
    VkDeviceSize bufferSize = sizeof(Vertex) * mesh->getVertexCount();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, mesh->getVertices().data(), (size_t) bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 *mesh->vertexBufferPointer(), *mesh->vertexBufferMemoryPointer());

    copyBuffer(stagingBuffer, mesh->vertexBuffer(), bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Renderer::createPointCloudVertexBuffer(PointCloud* pointCloud)
{
    const std::vector<Vertex>& vertices = pointCloud->getVertices();
    if (vertices.empty())
        return;

    const VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

    // Create a CPU-visible buffer for the upload
    VkBuffer stagingBuffer{ VK_NULL_HANDLE };
    VkDeviceMemory stagingBufferMemory{ VK_NULL_HANDLE };
    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer,
                 stagingBufferMemory);

    // Copy the prepared vertices into the staging buffer
    void* data = nullptr;
    if (vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data) != VK_SUCCESS)
        throw std::runtime_error("failed to map point-cloud staging memory!");

    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    // Create the final vertex buffer in fast GPU memory
    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 pointCloudVertexBuffer,
                 pointCloudVertexBufferMemory);

    // Upload the vertices and release the temporary buffer
    copyBuffer(stagingBuffer, pointCloudVertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Renderer::createIndexBuffer(Mesh *mesh)
{
    VkDeviceSize bufferSize = sizeof(int32_t) * mesh->getIndexCount();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, mesh->indices().data(), (size_t) bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 *mesh->indexBufferPointer(), *mesh->indexBufferMemoryPointer());

    copyBuffer(stagingBuffer, mesh->indexBuffer(), bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Renderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("failed to create buffer!");
    else
        LOG("created buffer");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate buffer memory!");
    else
        LOG("allocated buffer memory");

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

// Texture update:
void Renderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

void Renderer::createUniformBuffers()
{
    LOG("creating UniformBuffers");
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);

        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}

void Renderer::updateUniformBuffer(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    // auto currentTime = std::chrono::high_resolution_clock::now();
    // float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.view = glm::lookAt(mCamera->mPosition, mCamera->mPosition + mCamera->mForward, mCamera->mUp);
    ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float) swapChainExtent.height, 0.1f, 10000.0f);
    ubo.proj[1][1] *= -1;
    ubo.cameraPosition = glm::vec4(mCamera->mPosition, 0.f);
    // ubo.lightPosition =

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void Renderer::createDescriptorPool()
{
    // Uniform Descriptor Pool (array of 1 for now)
    std::array<VkDescriptorPoolSize, 1> uniformPoolSize{};
    uniformPoolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformPoolSize[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo uniformPoolInfo{};
    uniformPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    uniformPoolInfo.poolSizeCount = static_cast<uint32_t>(uniformPoolSize.size());
    uniformPoolInfo.pPoolSizes = uniformPoolSize.data();
    uniformPoolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(device, &uniformPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor pool!");
    else
        LOG("created Uniform descriptor pool");


    // Separate Sampler Descriptor Pool == Texture Sampler Pool
    // This is not the best and optimal way to do it, but works for us now
    VkDescriptorPoolSize samplerPoolSize{};
    samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerPoolSize.descriptorCount = MAX_TEXTURES;  // Implying max one texture pr object
                                                    // DescriptorSetSamplersCount can be around 1 000 000!

    VkDescriptorPoolCreateInfo samplerPoolInfo{};
    samplerPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    samplerPoolInfo.poolSizeCount = 1;      //Only one samplerPoolInfo - pool
    samplerPoolInfo.pPoolSizes = &samplerPoolSize;
    samplerPoolInfo.maxSets = MAX_TEXTURES;

    if (vkCreateDescriptorPool(device, &samplerPoolInfo, nullptr, &samplerDescriptorPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create sampler descriptor pool!");
    else
        LOG("created Sampler descriptor pool");
}

void Renderer::createDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate descriptor sets!");

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        // descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        // descriptorWrites[1].dstSet = descriptorSets[i];
        // descriptorWrites[1].dstBinding = 1;
        // descriptorWrites[1].dstArrayElement = 0;
        // descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        // descriptorWrites[1].descriptorCount = 1;
        // descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }
}

void Renderer::createTextureDescriptor(Texture *textureIn)
{
    VkDescriptorSet descriptorSet;

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = samplerDescriptorPool;
    allocInfo.descriptorSetCount = 1; // called once pr image
    allocInfo.pSetLayouts = &samplerSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate texture descriptor set!");
    else
        LOG("allocate texture descriptor set");

    // Texture update
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureIn->mTextureImageView;
    imageInfo.sampler = mTextureSampler;

    VkWriteDescriptorSet descriptorWrites{};
    descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites.dstSet = descriptorSet;
    descriptorWrites.dstBinding = 0;
    descriptorWrites.dstArrayElement = 0;
    descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites.descriptorCount = 1;
    descriptorWrites.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);

    // Add to vector of images;
    samplerDescriptorSets.push_back(descriptorSet);
}

void Renderer::drawFrame()
{
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("failed to acquire swap chain image!");

    // If a previous frame is still using this image, wait for it to finish before we reuse its resources
    if (imagesInFlight.size() > 0 && imagesInFlight[imageIndex] != VK_NULL_HANDLE)
    {
        vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    // Mark this image as now being used by the current frame
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    // DescriptorSet update
    updateUniformBuffer(currentFrame);

    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    vkResetCommandBuffer(commandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    // Use render-finished semaphore associated with the acquired image
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[imageIndex] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
        throw std::runtime_error("failed to submit draw command buffer!");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    // Wait on the same per-image render-finished semaphore
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
    {
        framebufferResized = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS)
        throw std::runtime_error("failed to present swap chain image!");

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

VkShaderModule Renderer::createShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        throw std::runtime_error("failed to create shader module!");
    else
        LOG("   created shader module");

    return shaderModule;
}

VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR Renderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            return availablePresentMode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;
    else
    {
        //If value can vary, need to set manually
        //Get the size of the render window - Qt style
        //GLFW uses glfwGetFramebufferSize(window, &width, &height);
        int width = this->width();
        int height = this->height();

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

// Query the supported swap chain details
SwapChainSupportDetails Renderer::querySwapChainSupport(VkPhysicalDevice device)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

bool Renderer::isDeviceSuitable(VkPhysicalDevice device)
{
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    // Check for basic swapchain support (formats and present modes)
    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy && supportedFeatures.fillModeNonSolid;
}

bool Renderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices Renderer::findQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport)
            indices.presentFamily = i;

        // MacOS MoltenVK automatically supports presentation on the same queue as graphics
        if (indices.isComplete())
            break;

        i++;
    }

    return indices;
}

std::vector<const char *> Renderer::getRequiredExtensions()
{
    //To finsih of the createInfo struct, we need to set up some more data
    //Since we don't use GLFW, this part is a little more verbose
    //GLFW hides parts of this.

    //Create a vector to hold the extensions
    std::vector<const char*> extensions = std::vector<const char*>();

    uint32_t extensionCount{ 0 };
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (result != VK_SUCCESS)
        qWarning("Failed to enumerate Vulkan instance extensions!");
    else
        LOGP("\nVulkan instance extension count: %u", extensionCount);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    // Log all available extensions
    LOG("Available Vulkan extensions:");
    for (const auto& ext : availableExtensions)
        LOG(ext.extensionName);

    extensions.push_back("VK_KHR_surface");

#ifdef _WIN32
    extensions.push_back("VK_KHR_win32_surface");	// Only on Windows

#elif defined(Q_OS_LINUX)
    instanceExtensions.push_back("VK_KHR_xcb_surface");		// or xlib_surface, depending on your Qt build

#elif defined(__APPLE__)
    extensions.push_back("VK_MVK_macos_surface");
    extensions.push_back("VK_MVK_moltenvk");
    //extensions.push_back("VK_EXT_METAL_SURFACE_EXTENSION_NAME");
#endif

    // If validation is enabled, add extension to report validation debug info
    if (enableValidationLayers)
         extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

bool Renderer::checkValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers)
    {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
            return false;
    }

    return true;
}

VkBool32 Renderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                 VkDebugUtilsMessageTypeFlagsEXT messageType,
                                 const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                 void *pUserData)
{
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void Renderer::setMainWindow(MainWindow *mainWindowIn)
{
    mMainWindow = mainWindowIn;
}

// Vertex buffer update:
uint32_t Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    // for-loop should return before we end here, and then it is an error
    throw std::runtime_error("failed to find suitable memory type!");
}

// Helper functions - Texture update
VkCommandBuffer Renderer::beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

// Texture update
void Renderer::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

//Texture update
void Renderer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                                     VkImageLayout newLayout, uint32_t mipLevels)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;    //MipMap update
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
        throw std::invalid_argument("unsupported layout transition!");

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
        );

    endSingleTimeCommands(commandBuffer);
}

// Texture update
void Renderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

// MipMap update
void Renderer::generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
        throw std::runtime_error("texture image format does not support linear blitting!");


    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++)
    {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
                       image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit,
                       VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

VkSampleCountFlagBits Renderer::getMaxUsableSampleCount()
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}



// ***** Qt specific functions: *****

void Renderer::exposeEvent(QExposeEvent *event)
{
//Called also on resize
#ifdef _WIN32   // These lines does not work on macOS so need this test
    LOG("exposeEvent called");
    if (isExposed())
        drawFrame();			//actual drawing
#endif
}

void Renderer::resizeEvent(QResizeEvent *event)
{
    LOGH("resizeEvent called");
    if (isExposed())
        drawFrame();			//actual drawing
}

bool Renderer::event(QEvent *event)
{
    // The event is to Update, and the program is visible
    if (event->type() == QEvent::UpdateRequest && isExposed())
    {
        // if(mEngine)
        {
            mEngine->update();		//calls drawFrame() in the end
            return true;
        }
    }

    return QWindow::event(event);
}

// Just sending the key and mouse input back to MainWindow to handle all in one place
void Renderer::keyPressEvent(QKeyEvent *event)
{
    mMainWindow->keyPressEvent(event);
}

void Renderer::keyReleaseEvent(QKeyEvent *event)
{
    mMainWindow->keyReleaseEvent(event);
}

void Renderer::mousePressEvent(QMouseEvent *event)
{
    mMainWindow->mousePressEvent(event);
}

void Renderer::mouseReleaseEvent(QMouseEvent *event)
{
    mMainWindow->mouseReleaseEvent(event);
}

void Renderer::mouseMoveEvent(QMouseEvent *event)
{
    mMainWindow->mouseMoveEvent(event);
}

void Renderer::wheelEvent(QWheelEvent *event)
{
    mMainWindow->wheelEvent(event);
}

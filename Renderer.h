#ifndef RENDERER_H
#define RENDERER_H

#include "Logger.h"
#include "Utilities.h"
#include <QWindow>
#include <vulkan/vulkan_core.h>
#include <vector>
#include <glm/glm.hpp>

//Forward declarations
struct SwapChainSupportDetails;
struct QueueFamilyIndices;
struct Texture;
class Camera;
class Light;
class Engine;
class MainWindow;
class Mesh;

class Renderer : public QWindow
{
    Q_OBJECT
public:
    explicit Renderer(QWindow* parent = nullptr);
    ~Renderer();

    void initVulkan();

    void setMainWindow(class MainWindow *mainWindowIn);

    Camera* camera() { return mCamera; };

    void update();

protected:
    // Qt event handlers - called when requestUpdate(); is called
    void exposeEvent(QExposeEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool event(QEvent* event) override;

    // Qt captures keyPresses
    // Camera update:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;;

private:
    // pushConstants update:
    PushConstants mPushConstants{};

    Camera* mCamera{ nullptr };
    Light* mLight{ nullptr };
    Engine* mEngine { nullptr };
    MainWindow* mMainWindow{ nullptr };

    //Base variables directly from Vulkan-Tutorial.com
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkDevice device;

    // Multisample update:
    VkSampleCountFlagBits msaaSamples{VK_SAMPLE_COUNT_1_BIT};
    VkImage colorImage;
    VkDeviceMemory colorImageMemory;
    VkImageView colorImageView;

    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass renderPass;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSetLayout samplerSetLayout;         // For textures
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkPipeline graphicsPipeline2;

    VkCommandPool commandPool;

    // Depth buffer update
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    VkDescriptorPool descriptorPool;
    VkDescriptorPool samplerDescriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;            // one for each swapchain image
    std::vector<VkDescriptorSet> samplerDescriptorSets;     // one for each texture!

    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight; // one fence per swapchain image, used to track previous usage
    uint32_t currentFrame = 0;

    bool framebufferResized = false;

    friend class Mesh; // Allow Mesh to access private members of Renderer for buffer creation
    friend class Engine; // Allow Engine to access private members of Renderer for buffer creation
	friend class MainWindow; // Allow MainWindow to access private members of Renderer

    bool mRenderLines{ false }; // For toggling between line and fill mode in the pipeline

    // Textures:
    VkSampler mTextureSampler;  //using same sampler for all images. Probably no need for one for each

    // ---- Above From Vulkan-Tutorial.com ----

    // Our own logger in the MainWindow
    // set in the constructor
    class Logger& mLogger;
    // Logger manipulator - OBS: variable name might clash!
    LineEnd endl;

    // ---- Functions from Vulkan-Tutorial.com ----
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createTextureImage(Texture *textureIn);
    void createTextureImageView(Texture *textureIn);
    void createTextureSampler();    //Texture *textureIn);
    void createColorResources();    //Multisampling update
    void createDepthResources();
    void createCommandBuffers();
    void createSyncObjects();
    void createVertexBuffer(Mesh *mesh);
    void createIndexBuffer(Mesh* mesh);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createTextureDescriptor(Texture *textureIn);
    void updateUniformBuffer(uint32_t currentImage);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void cleanupSwapChain();
    void recreateSwapChain();

    // Depth buffer update:
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples,
                     VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
    bool hasStencilComponent(VkFormat format);

    // Texture update:
    // Helper functions
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    // MipMap update:
    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    // Multisampling update:
    VkSampleCountFlagBits getMaxUsableSampleCount();

    void drawFrame();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    std::vector<const char *> getRequiredExtensions();
    bool checkValidationLayerSupport();
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                        void* pUserData);

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    void cleanup();
 };

#endif // RENDERER_H

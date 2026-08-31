#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <vulkan/vulkan_core.h>

struct Texture
{
    Texture(std::string textureName) : mTextureName{textureName} {};

    std::string mTextureName;

    int mTexWidth{0};
    int mTexHeight{0};
    int mTexChannels{0};
    uint32_t mMipLevels;

    VkImage mTextureImage;
    VkDeviceMemory mTextureImageMemory;
    VkImageView mTextureImageView;
    // VkSampler mTextureSampler;       //using the same sampler for all textures
};

#endif // TEXTURE_H

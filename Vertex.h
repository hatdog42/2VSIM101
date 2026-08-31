#ifndef VERTEX_H
#define VERTEX_H

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>
#include <array>

// Very similar to what is discussed here:
// https://vulkan-tutorial.com/Vertex_buffers/Vertex_input_description

// To be 100% similar to the tutorial it is a struct for now - not propper OOP!

struct Vertex
{
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 textureCoordinate;  // have Textures now!

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex); //the data size before the next vertex
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, textureCoordinate);

        return attributeDescriptions;
    }

};

#endif // VERTEX_H

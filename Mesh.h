#ifndef MESH_H
#define MESH_H

#include <string>
#include <vulkan/vulkan_core.h>
#include <vector>
#include "Vertex.h"

//Forward declaration
class Renderer;
class Logger;

// Mesh class represents the vertices of a 3D mesh object
class Mesh
{
public:
    // Enum for different meshes
    enum class MeshType { TRIANGLE = 0, QUAD, OBJ, NONE };

    Mesh(Renderer *render);
    Mesh(Renderer *render, MeshType meshTypeIn, std::string fileNameIn = "");
    ~Mesh();

    void makeTriangle();
    void makeQuad();
    void makeObj();

    void createBuffers(); // Function to create vertex and index buffers for this mesh

    bool hasIndices() const { return !mIndices.empty(); }

    VkBuffer vertexBuffer() const { return mVertexBuffer; }
    VkBuffer* vertexBufferPointer() { return &mVertexBuffer; }
    VkDeviceMemory vertexBufferMemory() const { return mVertexBufferMemory; }
    VkDeviceMemory* vertexBufferMemoryPointer() { return &mVertexBufferMemory; }

    const std::vector<Vertex>& getVertices() const { return mVertices; }

    size_t getVertexCount() const { return mVertices.size(); }
    size_t getIndexCount() const { return mIndices.size(); }

    VkBuffer indexBuffer() const { return mIndexBuffer; }
    VkBuffer* indexBufferPointer() { return &mIndexBuffer; }
    VkDeviceMemory indexBufferMemory() { return mIndexBufferMemory; }
    VkDeviceMemory* indexBufferMemoryPointer() { return &mIndexBufferMemory; }
    const std::vector<uint32_t>& indices() const { return mIndices; };

protected:
    // Initialize Vulkan handles to VK_NULL_HANDLE to avoid destroying uninitialized values
    VkBuffer mVertexBuffer{ VK_NULL_HANDLE };
    VkDeviceMemory mVertexBufferMemory{ VK_NULL_HANDLE };
    VkBuffer mIndexBuffer{ VK_NULL_HANDLE };
    VkDeviceMemory mIndexBufferMemory{ VK_NULL_HANDLE };

    std::vector<Vertex> mVertices;  // Holds the vertex data for the mesh
    std::vector<uint32_t> mIndices; // Holds the index data for the mesh (if any).

    std::string mFileName;  // Used for identifying the mesh, e.g. when loading from an OBJ file

    // Pointer / reference to other classes used in functions
    Renderer* mRenderer{nullptr};
    Logger& mLogger;
};

#endif // MESH_H

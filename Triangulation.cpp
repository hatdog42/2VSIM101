#include "PointCloud.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <utility>

bool PointCloud::loadTriangulation(const std::string& filePath)
{
    mTriangleIndices.clear();
    std::ifstream file(filePath);
    std::size_t triangleCount = 0;
    if (mVertices.empty() || !(file >> triangleCount) || triangleCount == 0 ||
        triangleCount > std::numeric_limits<uint32_t>::max() / 3)
    {
        std::cerr << "Cannot load triangulation: " << filePath << '\n';
        return false;
    }

    std::vector<uint32_t> indices;
    indices.reserve(triangleCount * 3);
    for (std::size_t triangle = 0; triangle < triangleCount; ++triangle)
    {
        // Each row contains three vertex indices followed by three neighbours
        long long a, b, c, neighbourA, neighbourB, neighbourC; // long long for more information
        if (!(file >> a >> b >> c >> neighbourA >> neighbourB >> neighbourC))
        {
            std::cerr << "Invalid triangle at row " << triangle + 1 << '\n';
            return false;
        }
        indices.push_back(static_cast<uint32_t>(a));
        indices.push_back(static_cast<uint32_t>(b));
        indices.push_back(static_cast<uint32_t>(c));
    }
    // colores the terain based on hight
    float minHeight = mVertices.front().position.y;
    float maxHeight = minHeight;
    for (const Vertex& vertex : mVertices)
    {
        minHeight = std::min(minHeight, vertex.position.y);
        maxHeight = std::max(maxHeight, vertex.position.y);
    }
    const float heightRange = maxHeight - minHeight;
    for (Vertex& vertex : mVertices)
    {
        const float t = heightRange > 0.0f ? (vertex.position.y - minHeight) / heightRange : 0.5f;
        vertex.color = glm::mix(glm::vec3(0.1f, 0.3f, 0.1f),glm::vec3(0.9f, 0.8f, 0.5f), t);
    }

    mTriangleIndices = std::move(indices);
    std::cout << "Loaded " << triangleCount << " triangles\n";
    return true;
}

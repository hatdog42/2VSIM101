#include "PointCloud.h"

#include <fstream>
#include <iostream>
#include <utility>

bool PointCloud::loadFromFile(const std::string& filePath)
{
    std::ifstream file(filePath);

    if (!file.is_open())
    {
        std::cerr << "Could not open file: " << filePath << '\n';
        return false;
    }

    std::size_t expectedPointCount = 0;

    // The first value says how many points the file contains
    if (!(file >> expectedPointCount))
    {
        std::cerr << "Could not read point count\n";
        return false;
    }

    mPoints.clear();
    mVertices.clear();
    mTriangleIndices.clear();
    mOrigin = {}; //
    mPoints.reserve(expectedPointCount); // Reserve memory for the expected points

    Point point{}; // Temporary point used while reading

    // Read each point as three coordinates as in the text file
    while (file >> point.x >> point.y >> point.z)
    {
        mPoints.push_back(point);
    }

    if (mPoints.size() != expectedPointCount)
    {
        std::cerr << "Warning: Expected "<< expectedPointCount
        << " points, but loaded "<< mPoints.size()<< '\n';
    }

    std::cout << "Loaded " << mPoints.size() << " points\n";

    return true;
}

bool PointCloud::prepareVertices()
{
    if (mPoints.empty())
    {
        std::cerr << "Cannot prepare vertices: no points are loaded\n";
        return false;
    }

    // Use a local origin to make the float coordinates smaller
    mOrigin = mPoints[0];

    std::vector<Vertex> preparedVertices;
    preparedVertices.reserve(mPoints.size());

    for (const Point& point : mPoints)
    {
        // Subtract the origin to convert to local coordinates
        const double localX = point.x - mOrigin.x;
        const double localY = point.y - mOrigin.y;
        const double localZ = point.z - mOrigin.z;

        // Swap Y and Z and convert float to match the engine's coordinate system
        Vertex vertex{};
        vertex.position = glm::vec3(
            static_cast<float>(localX),
            static_cast<float>(localZ),
            static_cast<float>(localY));
        vertex.color = glm::vec3(1.0f);
        vertex.textureCoordinate = glm::vec2(0.0f);

        preparedVertices.push_back(vertex);
    }

    mVertices = std::move(preparedVertices);

    std::cout << "Prepared " << mVertices.size() << " vertices\n";
    return true;
}

const std::vector<Point>& PointCloud::getPoints() const
{
    return mPoints;
}

std::size_t PointCloud::getPointCount() const
{
    return mPoints.size();
}

const std::vector<Vertex>& PointCloud::getVertices() const
{
    return mVertices;
}

const Point& PointCloud::getOrigin() const
{
    return mOrigin;
}

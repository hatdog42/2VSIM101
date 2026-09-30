#pragma once

#include "Vertex.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct Point
{
    double x;
    double y;
    double z;
};

class PointCloud
{
public:
    bool loadFromFile(const std::string& filePath);
    bool prepareVertices();

    bool loadTriangulation(const std::string& filePath); //used in triangulation.cpp
    [[nodiscard]] const std::vector<uint32_t>& getTriangleIndices() const { return mTriangleIndices; }

    [[nodiscard]] const std::vector<Point>& getPoints() const;
    [[nodiscard]] std::size_t getPointCount() const;
    [[nodiscard]] const std::vector<Vertex>& getVertices() const;
    [[nodiscard]] const Point& getOrigin() const;

private:
    std::vector<Point> mPoints;
    std::vector<Vertex> mVertices;
    std::vector<uint32_t> mTriangleIndices;
    Point mOrigin{};
};

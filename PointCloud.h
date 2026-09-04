#pragma once

#include "Vertex.h"

#include <cstddef>
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

    [[nodiscard]] const std::vector<Point>& getPoints() const;
    [[nodiscard]] std::size_t getPointCount() const;
    [[nodiscard]] const std::vector<Vertex>& getVertices() const;
    [[nodiscard]] const Point& getOrigin() const;

private:
    std::vector<Point> mPoints;
    std::vector<Vertex> mVertices;
    Point mOrigin{};
};

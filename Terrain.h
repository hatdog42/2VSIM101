#ifndef TERRAIN_H
#define TERRAIN_H

#include "Mesh.h"

class Renderer;

class Terrain : public Mesh
{
public:
    Terrain(Renderer* render, int gridSize = 2, int scale = 1);

    float heightAtPoint(const glm::vec2& point);

private:
    glm::vec3 barycentricCoordinates(const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3, glm::vec2 testPoint);
    void calculateHeighMapNormals(int width, int depth);

    // how much to scale the terrain by (distance between vertices)
    int mScale{1};
};

#endif // TERRAIN_H

#include "Physics.h"
#include "PointCloud.h"
#include <cmath>
int Physics::findTriangle(const PointCloud& surface, const glm::vec3& position, const int previusTriangle)
{
    int triangle = previusTriangle;

    const auto& vertices = surface.getVertices();
    const auto& indices = surface.getTriangleIndices();
    const auto& neighbours = surface.getNeighbours();

    const glm::vec2 p(position.x, position.z); // y is up
    constexpr float epsilon = 0.00001f;

    int triangleCount = static_cast<int>(indices.size() / 3);

    for (int steps = 0; steps < triangleCount; ++steps)
    {
        if (triangle < 0 || triangle >= triangleCount) return -1; //no neigbour / out of bounds

        int firstIndex = 3 * triangle;

        // get triangle
        const glm::vec3& A = vertices[indices[firstIndex]].position;
        const glm::vec3& B = vertices[indices[firstIndex + 1]].position;
        const glm::vec3& C = vertices[indices[firstIndex + 2]].position;

        // only need x and z to find the trianbgle
        const glm::vec2 a(A.x, A.z);
        const glm::vec2 b(B.x, B.z);
        const glm::vec2 c(C.x, C.z);

        const float area = Cross(b - a, c - a);

        if (std::abs(area) < epsilon) return -1;

        //barysentrick cordinets
        const float u = Cross(b - p, c - p) / area;
        const float v = Cross(c - p, a - p) / area;
        const float w = 1.0f - u - v;

        if (u >= -epsilon && v >= -epsilon && w >= -epsilon) // is inside the triangle
        {
            return triangle;
        }

        if (u <= v && u <= w)triangle = neighbours[triangle][0];
        else if (v <= w)triangle = neighbours[triangle][1];
        else triangle = neighbours[triangle][2];
    }

    return -1;
}

glm::vec3 Physics::calculateSlidingAcceleration(const glm::vec3& A,const glm::vec3& B,const glm::vec3& C)
{
    glm::vec3 gravity(0.0f, -9.81f, 0.0f);

    glm::vec3 edge1 = B - A; //edges of triangles
    glm::vec3 edge2 = C - A;

    glm::vec3 n = glm::cross(edge1, edge2); //cross product to get normal vector

    float length = glm::length(n); // chek if triangle is good
    if (length < 0.0001) return glm::vec3(0.0f);

    glm::vec3 normal = n / length; //  n / ||n||

    if (normal.y < 0.0f) normal = -normal;

    glm::vec3 acceleration = gravity - glm::dot(gravity, normal) * normal; // get acselaration down slope.  g - force(scalar) * dir of force
    return acceleration;
}

void Physics::ChangeTriangle(glm::vec3& position,glm::vec3& velocity,float radius,const glm::vec3& A,const glm::vec3& B,const glm::vec3& C)
{
    glm::vec3 n = glm::cross(B - A, C - A);
    float length = glm::length(n);

    if (length < 0.0001f) return;

    glm::vec3 normal = n / length;
    if (normal.y < 0.0f) normal = -normal;

    // correct the ball distance above the surface
    float distance = glm::dot(position - A, normal);
    position += (radius - distance) * normal;

    // make velocity parallel to the surface
    velocity -= glm::dot(velocity, normal) * normal;
}

void Physics::Move( glm::vec3& position, glm::vec3& velocity,const glm::vec3& acceleration, float dt)
{
    position += velocity * dt + 0.5f * acceleration * dt * dt;
    velocity += acceleration * dt;
}

float Physics::Cross(glm::vec2 u, glm::vec2 v)
{
    return u.x * v.y - u.y * v.x;
}
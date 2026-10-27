#ifndef LASDATA_TXT_PHYSICS_H
#define LASDATA_TXT_PHYSICS_H

#include <glm/vec3.hpp>
#include "PointCloud.h"


class Physics
{
    public:
    int findTriangle(const PointCloud& surface, const glm::vec3& position, const int previusTriangle);

    glm::vec3 calculateSlidingAcceleration(const glm::vec3& A,const glm::vec3& B,const glm::vec3& C);

    void ChangeTriangle(glm::vec3& position,glm::vec3& velocity,float radius,const glm::vec3& A,const glm::vec3& B,const glm::vec3& C);

    void Move( glm::vec3& position, glm::vec3& velocity,const glm::vec3& acceleration, float dt);

    float Cross(glm::vec2 u, glm::vec2 v);
};


#endif //LASDATA_TXT_PHYSICS_H

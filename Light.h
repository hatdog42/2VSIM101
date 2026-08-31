#ifndef LIGHT_H
#define LIGHT_H

#include "Transform.h"

class Light
{
public:
    Light();

    Transform mTransform;

    glm::vec3 lightColor{glm::vec3(1.0, 0.9, 0.5)};     // pale yellow

    float ambientStrength = 0.1f;
    float specularStrength = 0.5f;
    int specularExponent = 256;
};

#endif // LIGHT_H

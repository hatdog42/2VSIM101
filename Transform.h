#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Transform
{
    glm::vec3 position;
    glm::vec3 rotation;
    // defaults to 1 in size - zero will make it disappear!
    glm::vec3 scale{glm::vec3(1.f, 1.f, 1.f)};

    // not optimized, but correct?
    glm::mat4 TransformMatrix() const
    {
        const glm::mat4 T = glm::translate(glm::mat4(1.0f), position);

        const glm::mat4 R =
            glm::rotate(glm::mat4(1.0f), glm::radians(rotation.y), glm::vec3(0,1,0)) * // yaw
            glm::rotate(glm::mat4(1.0f), glm::radians(rotation.x), glm::vec3(1,0,0)) * // pitch
            glm::rotate(glm::mat4(1.0f), glm::radians(rotation.z), glm::vec3(0,0,1));  // roll

        const glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);

        return T * R * S;
    }
};

#endif // TRANSFORM_H

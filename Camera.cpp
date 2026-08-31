#include "Camera.h"
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

Camera::Camera() {}

void Camera::resetMovement()
{
    mCameraMovement = glm::vec3{0};
}

void Camera::update()
{
    // ***** Rotation ***** :
    // no roll, since we don't want that for now

    // Rotate forward around X == Pitch
    mForward = glm::rotate(glm::vec3{0.f, 0.f, -1.f}, glm::radians(mPitch), glm::vec3{1.0f, 0.0f, 0.0f});

    // Rotate forward around world up == Yaw
    mForward = glm::rotate(mForward, glm::radians(mYaw), glm::vec3{0.0f, 1.0f, 0.0f});

    // ***** Movement ***** :

    // move camera along the forward vector:
    mPosition += mForward * mCameraMovement.z;

    // the local right vector of the camera:
    mRight = glm::rotate(glm::vec3{1, 0, 0}, glm::radians(mYaw), glm::vec3{0.0f, 1.0f, 0.0f});
    // move camera along the right vector:
    mPosition += mRight * mCameraMovement.x;

    // move camera along worldUp vector
    mPosition += mUp * mCameraMovement.y;
}

#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>

class Camera
{
public:
    Camera();

    // Reset movement from last frame
    void resetMovement();

    // Update position, forward and right vector based on input
    void update();

    // Initial values - will quicly be overwritten
    glm::vec3 mPosition {7.0f, 10.0f, 18.0f};
    glm::vec3 mForward {0, 0, -1};      // Forward vector local to the camera
    glm::vec3 mRight {1, 0, 0};         // Right vector local to the camera
    glm::vec3 mUp {0, 1, 0};            // Using +Y up

    // These values holds absolute values:
    float mPitch {-30.f};               // The pitch of the camera - in Euler angle
    float mYaw {0.f};                   // The yaw of the camera - in Euler angle

    // How much should the camera move next frame in cameraForward coordinates
    glm::vec3 mCameraMovement{0, 0, 0};

};

#endif // CAMERA_H

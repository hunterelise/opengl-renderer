#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

class OrbitCamera
{
public:
    // The point in world space that the camera always looks at and rotates around.
    glm::vec3 target{ 0.0f, 0.0f, 0.0f };

    // How far the camera is from the target. Zooming changes this value.
    float distance = 5.0f;

    // Rotation angles around the target.
    // Yaw rotates left and right, pitch rotates up and down.
    // Stored in radians to match GLM math functions.
    float yaw = glm::radians(45.0f);
    float pitch = glm::radians(-25.0f);

    // Sensitivity values that control how fast the camera responds to input.
    float orbitSpeed = 0.01f;
    float panSpeed = 0.002f;
    float zoomSpeed = 0.9f;

    glm::vec3 position() const
    {
        // Convert yaw and pitch into a position on a sphere around the target.
        // This keeps the camera at a fixed distance while allowing rotation.
        float cp = std::cos(pitch);
        glm::vec3 offset;
        offset.x = distance * cp * std::cos(yaw);
        offset.y = distance * std::sin(pitch);
        offset.z = distance * cp * std::sin(yaw);
        return target + offset;
    }

    glm::mat4 viewMatrix() const
    {
        // Build a view matrix that looks from the camera position toward the target.
        // The up direction is fixed to world up to avoid unwanted roll.
        return glm::lookAt(position(), target, glm::vec3(0, 1, 0));
    }

    void orbit(float dxPixels, float dyPixels)
    {
        // Mouse movement is converted into angle changes so the camera rotates around the target.
        yaw += dxPixels * orbitSpeed;
        pitch += dyPixels * orbitSpeed;

        // Limiting pitch prevents the camera from flipping upside down.
        const float limit = glm::radians(89.0f);
        pitch = std::clamp(pitch, -limit, limit);
    }

    void zoom(float scrollY)
    {
        // Zoom changes the distance using a multiplier so it feels smooth at any scale.
        if (scrollY > 0.0f) distance *= std::pow(zoomSpeed, scrollY);
        else                distance /= std::pow(zoomSpeed, -scrollY);

        // Keep the camera within a reasonable range to avoid precision and clipping issues.
        distance = std::clamp(distance, 0.5f, 100.0f);
    }

    void pan(float dxPixels, float dyPixels)
    {
        // Panning moves the target sideways and up relative to the camera view.
        // This makes it feel like you are dragging the scene under the cursor.
        const glm::vec3 pos = position();
        const glm::vec3 forward = glm::normalize(target - pos);
        const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));

        // Scale movement by distance so panning speed feels natural when zoomed in or out.
        const float scale = distance * panSpeed;
        target += (-dxPixels * scale) * right + (dyPixels * scale) * up;
    }
};

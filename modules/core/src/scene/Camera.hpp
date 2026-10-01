/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** First-person camera (position + yaw/pitch)
*/
#pragma once

#include "Matrix.hpp"
#include "Vector.hpp"
#include <numbers>

namespace k3
{

    enum class Projection {
        Perspective,
        Orthographic
    };

    // Right-handed, y up. With yaw = pitch = 0 the camera looks toward -Z.
    struct Camera
    {
        Math::Vector3f position{0.f, 0.f, 5.f};
        float yaw   = 0.f;                                  // Radians, positive turns right
        float pitch = 0.f;                                  // Radians, positive looks up

        Projection projection = Projection::Perspective;
        float fovY        = std::numbers::pi_v<float> / 3.f; // Perspective: vertical field of view (radians)
        float orthoHeight = 2.f;                            // Orthographic: visible height in world units
        float nearPlane   = 0.05f;
        float farPlane    = 100.f;

        [[nodiscard]] Math::Vector3f forward() const noexcept;
        [[nodiscard]] Math::Vector3f right() const noexcept;
        [[nodiscard]] Math::Vector3f up() const noexcept;

        [[nodiscard]] Math::Matrix4 view() const noexcept;
        [[nodiscard]] Math::Matrix4 projectionMatrix(float aspectRatio) const noexcept;

        // Sets yaw/pitch so the camera faces `target`.
        void lookAt(const Math::Vector3f& target) noexcept;
    };

}

#include <kronk3d/scene/Camera.hpp>
#include <algorithm>
#include <cmath>

k3::Math::Vector3f k3::Camera::forward() const noexcept
{
    return {
        std::sin(yaw) * std::cos(pitch),
        std::sin(pitch),
        -std::cos(yaw) * std::cos(pitch),
    };
}

namespace
{
    // Without the roll: the right of the view, and the up that goes with it
    k3::Math::Vector3f levelRight(float yaw) { return {std::cos(yaw), 0.f, std::sin(yaw)}; }
}

k3::Math::Vector3f k3::Camera::right() const noexcept
{
    const Math::Vector3f level = levelRight(yaw);

    if (roll == 0.f)
        return level;
    return std::cos(roll) * level - std::sin(roll) * Math::Vector3f::cross(level, forward());
}

k3::Math::Vector3f k3::Camera::up() const noexcept
{
    const Math::Vector3f level = Math::Vector3f::cross(levelRight(yaw), forward());

    if (roll == 0.f)
        return level;
    return std::cos(roll) * level + std::sin(roll) * levelRight(yaw);
}

k3::Math::Matrix4 k3::Camera::view() const noexcept
{
    return Math::Matrix4::lookAt(position, position + forward(), up());
}

k3::Math::Matrix4 k3::Camera::projectionMatrix(float aspectRatio) const noexcept
{
    if (projection == Projection::Orthographic) {
        const float halfHeight = orthoHeight * 0.5f;
        const float halfWidth = halfHeight * aspectRatio;
        return Math::Matrix4::orthographic(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
    }
    return Math::Matrix4::perspective(nearPlane, farPlane, fovY, aspectRatio);
}

void k3::Camera::lookAt(const Math::Vector3f& target) noexcept
{
    const auto d = Math::Vector3f::normalize(target - position);

    pitch = std::asin(std::clamp(d.y, -1.f, 1.f));
    yaw = std::atan2(d.x, -d.z);
}

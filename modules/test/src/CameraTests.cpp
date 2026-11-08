#include "Test.hpp"
#include <kronk3d/scene/Camera.hpp>
#include <cmath>
#include <numbers>

namespace
{
    using k3::Math::Vector3f;

    bool near(const Vector3f& a, const Vector3f& b) { return std::abs(a.x - b.x) < 1e-5f && std::abs(a.y - b.y) < 1e-5f && std::abs(a.z - b.z) < 1e-5f; }
}

K3_TEST(camera_without_roll_keeps_its_axes)
{
    k3::Camera camera;

    camera.yaw = 0.7f;
    camera.pitch = 0.3f;
    K3_CHECK(near(camera.right(), {std::cos(0.7f), 0.f, std::sin(0.7f)}));
    K3_CHECK(std::abs(camera.up().y) > 0.9f);
}

K3_TEST(camera_roll_turns_the_view_around_its_direction)
{
    k3::Camera level;
    k3::Camera rolled;

    rolled.yaw = level.yaw = 0.4f;
    rolled.pitch = level.pitch = -0.2f;
    rolled.roll = std::numbers::pi_v<float> * 0.5f;      // a quarter turn: the head on the right shoulder
    K3_CHECK(near(rolled.forward(), level.forward()));
    K3_CHECK(near(rolled.up(), level.right()));
    K3_CHECK(near(rolled.right(), -1.f * level.up()));
    K3_CHECK(std::abs(Vector3f::dot(rolled.up(), rolled.right())) < 1e-5f);
}

K3_TEST(camera_roll_tilts_the_horizon)
{
    k3::Camera camera;

    camera.roll = 0.5f;
    // A point to the right of the view, at the height of the eyes, is seen above the middle of the screen
    // when the head leans to the right (the world turns the other way, counter-clockwise)
    const Vector3f seen = camera.view().transformPoint({1.f, 0.f, 0.f});

    K3_CHECK(seen.y > 0.f);
    K3_CHECK(seen.x > 0.f);
}

#include "Test.hpp"
#include "render/Frustum.hpp"
#include <kronk3d/scene/Camera.hpp>

namespace
{

    constexpr float PI = 3.14159265f;

    k3::Math::Bounds3f box(k3::Math::Vector3f min, k3::Math::Vector3f max)
    {
        return {min, max};
    }

    // At the origin, looking toward -z.
    k3::Frustum perspective()
    {
        k3::Camera camera;

        camera.position = {0.f, 0.f, 0.f};
        camera.fovY = PI / 3.f;
        camera.nearPlane = 0.1f;
        camera.farPlane = 50.f;
        return k3::Frustum(camera.projectionMatrix(1.f) * camera.view());
    }

}

K3_TEST(frustum_keeps_what_is_in_front_of_the_camera)
{
    const auto frustum = perspective();

    K3_CHECK(frustum.intersects(box({-1.f, -1.f, -6.f}, {1.f, 1.f, -4.f})));
    K3_CHECK(frustum.intersects(box({-100.f, -100.f, -100.f}, {100.f, 100.f, 100.f})));     // Contains the camera
    K3_CHECK(frustum.intersects(box({2.f, -1.f, -6.f}, {50.f, 1.f, -4.f})));                // Straddles the right plane
}

K3_TEST(frustum_rejects_what_is_outside)
{
    const auto frustum = perspective();

    K3_CHECK(!frustum.intersects(box({-1.f, -1.f, 4.f}, {1.f, 1.f, 6.f})));                 // Behind
    K3_CHECK(!frustum.intersects(box({99.f, -1.f, -6.f}, {101.f, 1.f, -4.f})));             // Right
    K3_CHECK(!frustum.intersects(box({-101.f, -1.f, -6.f}, {-99.f, 1.f, -4.f})));           // Left
    K3_CHECK(!frustum.intersects(box({-1.f, 99.f, -6.f}, {1.f, 101.f, -4.f})));             // Above
    K3_CHECK(!frustum.intersects(box({-1.f, -101.f, -6.f}, {1.f, -99.f, -4.f})));           // Below
    K3_CHECK(!frustum.intersects(box({-1.f, -1.f, -61.f}, {1.f, 1.f, -51.f})));             // Past the far plane
    K3_CHECK(!frustum.intersects(k3::Math::Bounds3f{}));                                    // Empty
}

K3_TEST(frustum_follows_the_camera_orientation)
{
    k3::Camera camera;

    camera.position = {0.f, 0.f, 0.f};
    camera.yaw = PI / 2.f;          // Turned a quarter of a turn
    camera.farPlane = 50.f;
    const k3::Frustum frustum(camera.projectionMatrix(1.f) * camera.view());
    const k3::Math::Vector3f ahead = camera.forward() * 5.f;

    K3_CHECK(frustum.intersects(box(ahead - k3::Math::Vector3f{0.5f, 0.5f, 0.5f}, ahead + k3::Math::Vector3f{0.5f, 0.5f, 0.5f})));
    K3_CHECK(!frustum.intersects(box(-ahead - k3::Math::Vector3f{0.5f, 0.5f, 0.5f}, -ahead + k3::Math::Vector3f{0.5f, 0.5f, 0.5f})));
}

K3_TEST(frustum_handles_orthographic_cameras)
{
    k3::Camera camera;

    camera.position = {0.f, 0.f, 1.f};
    camera.projection = k3::Projection::Orthographic;
    camera.orthoHeight = 2.f;
    camera.nearPlane = 0.1f;
    camera.farPlane = 10.f;
    const k3::Frustum frustum(camera.projectionMatrix(1.f) * camera.view());

    K3_CHECK(frustum.intersects(box({-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f})));
    K3_CHECK(!frustum.intersects(box({1.5f, -0.5f, -0.5f}, {2.5f, 0.5f, 0.5f})));
    K3_CHECK(!frustum.intersects(box({-0.5f, -0.5f, 2.f}, {0.5f, 0.5f, 3.f})));             // Behind the camera
}

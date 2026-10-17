#include "Test.hpp"
#include <kronk3d/render/software/SoftwareRasterizer.hpp>
#include <cmath>

namespace
{

    constexpr std::uint32_t SIZE = 16;

    k3::Camera orthoCamera(const k3::Math::Vector3f& position, float pitch)
    {
        k3::Camera camera;

        camera.position = position;
        camera.pitch = pitch;
        camera.projection = k3::Projection::Orthographic;
        camera.orthoHeight = 4.f;
        camera.nearPlane = 0.1f;
        camera.farPlane = 20.f;
        return camera;
    }

    // Quads seen from above, from below and from the side: counter-clockwise from the viewer, no normals,
    // so the face normal is +y, -y and +z.
    std::shared_ptr<k3::Mesh> facing(const char* where)
    {
        const std::string_view w = where;

        if (w == "up")
            return std::make_shared<k3::Mesh>(k3::Mesh{.positions = {{-3.f, 0.f, 3.f}, {3.f, 0.f, 3.f}, {3.f, 0.f, -3.f}, {-3.f, 0.f, -3.f}}, .indices = {0, 1, 2, 0, 2, 3}});
        if (w == "down")
            return std::make_shared<k3::Mesh>(k3::Mesh{.positions = {{-3.f, 0.f, -3.f}, {3.f, 0.f, -3.f}, {3.f, 0.f, 3.f}, {-3.f, 0.f, 3.f}}, .indices = {0, 1, 2, 0, 2, 3}});
        return std::make_shared<k3::Mesh>(k3::Mesh{.positions = {{-3.f, -3.f, 0.f}, {3.f, -3.f, 0.f}, {3.f, 3.f, 0.f}, {-3.f, 3.f, 0.f}}, .indices = {0, 1, 2, 0, 2, 3}});
    }

    k3::Math::Color center(const k3::Environment& environment, const char* where, const k3::Camera& camera)
    {
        k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

        rasterizer.beginFrame(camera, environment);
        rasterizer.draw(facing(where), std::make_shared<k3::Material>(), k3::Math::Matrix4::identity());
        rasterizer.endFrame();
        return rasterizer.colorBuffer()[SIZE / 2 * SIZE + SIZE / 2];
    }

    // A plane whose vertex normals lean away from the corners, under a light shining at 45 degrees.
    std::vector<float> lightFromPlane(bool flat)
    {
        auto mesh = std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{-2.f, 0.f, 2.f}, {2.f, 0.f, 2.f}, {2.f, 0.f, -2.f}, {-2.f, 0.f, -2.f}},
            .normals = {k3::Math::Vector3f::normalize({-1.f, 1.f, 1.f}), k3::Math::Vector3f::normalize({1.f, 1.f, 1.f}),
                        k3::Math::Vector3f::normalize({1.f, 1.f, -1.f}), k3::Math::Vector3f::normalize({-1.f, 1.f, -1.f})},
            .indices = {0, 1, 2, 0, 2, 3},
        });
        auto material = std::make_shared<k3::Material>();
        k3::Environment environment;
        k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

        material->flatShading = flat;
        environment.ambient = k3::Math::Color::Black;
        environment.lights = {k3::Light::directional({1.f, -1.f, 0.f})};
        rasterizer.beginFrame(orthoCamera({0.f, 5.f, 0.f}, -1.5707963f), environment);
        rasterizer.draw(mesh, material, k3::Math::Matrix4::identity());
        rasterizer.endFrame();

        std::vector<float> values;
        for (const auto& pixel : rasterizer.colorBuffer())
            values.push_back(pixel.r);
        return values;
    }

}

K3_TEST(flat_shading_lights_each_triangle_with_its_own_normal)
{
    const auto smooth = lightFromPlane(false);
    const auto flat = lightFromPlane(true);
    const auto [lowSmooth, highSmooth] = std::minmax_element(smooth.begin(), smooth.end());

    K3_CHECK(*highSmooth - *lowSmooth > 0.05f);             // The vertex normals give a gradient
    for (const float value : flat)
        K3_REQUIRE(std::abs(value - 0.70710678f) < 1e-4f);  // The face normal (+y) against a light at 45 degrees: one value
}

K3_TEST(hemisphere_ambient_goes_from_the_sky_to_the_ground_with_the_normal)
{
    k3::Environment environment;
    const k3::Math::Color sky{1.f, 0.5f, 0.2f, 1.f}, ground{0.1f, 0.2f, 0.6f, 1.f};

    environment.ambient = sky;
    environment.ambientGround = ground;

    const auto up = center(environment, "up", orthoCamera({0.f, 5.f, 0.f}, -1.5707963f));
    const auto down = center(environment, "down", orthoCamera({0.f, -5.f, 0.f}, 1.5707963f));
    const auto side = center(environment, "side", orthoCamera({0.f, 0.f, 5.f}, 0.f));

    K3_CHECK_NEAR(up.r, sky.r, 1e-5f);
    K3_CHECK_NEAR(up.g, sky.g, 1e-5f);
    K3_CHECK_NEAR(up.b, sky.b, 1e-5f);
    K3_CHECK_NEAR(down.r, ground.r, 1e-5f);
    K3_CHECK_NEAR(down.b, ground.b, 1e-5f);
    K3_CHECK_NEAR(side.r, 0.5f * (sky.r + ground.r), 1e-5f);        // Halfway on a vertical surface
    K3_CHECK_NEAR(side.b, 0.5f * (sky.b + ground.b), 1e-5f);
}

K3_TEST(ambient_is_the_same_everywhere_without_a_hemisphere)
{
    k3::Environment environment;

    environment.ambient = {0.3f, 0.4f, 0.5f, 1.f};

    for (const auto& [where, camera] : {std::pair{"up", orthoCamera({0.f, 5.f, 0.f}, -1.5707963f)},
                                        std::pair{"down", orthoCamera({0.f, -5.f, 0.f}, 1.5707963f)},
                                        std::pair{"side", orthoCamera({0.f, 0.f, 5.f}, 0.f)}}) {
        const auto c = center(environment, where, camera);

        K3_CHECK_NEAR(c.r, 0.3f, 1e-5f);
        K3_CHECK_NEAR(c.b, 0.5f, 1e-5f);
    }
}

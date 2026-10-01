#include "Test.hpp"
#include "render/software/SoftwareRasterizer.hpp"
#include <cmath>

namespace
{

    constexpr std::uint32_t SIZE = 32;          // 32 px over 4 world units: 8 px per unit
    constexpr std::size_t   MIDDLE_ROW = SIZE / 2 * SIZE;

    // Looking straight down at the floor from above.
    k3::Camera topCamera()
    {
        k3::Camera camera;
        camera.position = {0.f, 5.f, 0.f};
        camera.pitch = -1.5707963f;
        camera.projection = k3::Projection::Orthographic;
        camera.orthoHeight = 4.f;
        camera.nearPlane = 0.1f;
        camera.farPlane = 10.f;
        return camera;
    }

    // Horizontal quad of half-size `half` at height y, facing up.
    std::shared_ptr<k3::Mesh> plane(float half, float y)
    {
        return std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{-half, y, half}, {half, y, half}, {half, y, -half}, {-half, y, -half}},
            .normals = {{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}},
            .uvs = {{0.f, 1.f}, {1.f, 1.f}, {1.f, 0.f}, {0.f, 0.f}},
            .indices = {0, 1, 2, 0, 2, 3},
        });
    }

    // No ambient, one sun shining at 45 degrees toward +x: the shadow of something 1 unit above the
    // floor lands 1 unit further along +x.
    k3::Environment slantedSun(bool castShadows)
    {
        k3::Environment environment;
        environment.ambient = k3::Math::Color::Black;
        environment.lights = {k3::Light::directional({1.f, -1.f, 0.f})};
        environment.lights[0].castShadows = castShadows;
        return environment;
    }

    // A 4x4 floor under a 1x1 occluder floating 1 unit above its center. Returns the middle row.
    std::vector<float> renderMiddleRow(const k3::Environment& environment, std::shared_ptr<k3::Material> occluder)
    {
        k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

        rasterizer.beginFrame(topCamera(), environment);
        rasterizer.draw(plane(2.f, 0.f), std::make_shared<k3::Material>(), k3::Math::Matrix4::identity());
        if (occluder)
            rasterizer.draw(plane(0.5f, 1.f), occluder, k3::Math::Matrix4::identity());
        rasterizer.endFrame();

        std::vector<float> row;
        for (std::uint32_t x = 0; x < SIZE; ++x)
            row.push_back(rasterizer.colorBuffer()[MIDDLE_ROW + x].r);
        return row;
    }

    // Floor pixels at x = +1.25 (in the occluder's shadow) and x = -1.25 (in the sun).
    constexpr std::size_t SHADOWED = SIZE / 2 + 10;
    constexpr std::size_t LIT = SIZE / 2 - 10;
    constexpr float       SUN = 0.70710678f;    // cos 45 degrees

}

K3_TEST(shadow_occluder_darkens_the_floor_behind_it)
{
    const auto row = renderMiddleRow(slantedSun(true), std::make_shared<k3::Material>());

    K3_CHECK(row[SHADOWED] == 0.f);
    K3_CHECK_NEAR(row[LIT], SUN, 1e-4f);
}

K3_TEST(shadow_needs_cast_shadows_on_light_and_material)
{
    K3_CHECK_NEAR(renderMiddleRow(slantedSun(false), std::make_shared<k3::Material>())[SHADOWED], SUN, 1e-4f);

    auto noCaster = std::make_shared<k3::Material>();
    noCaster->castShadows = false;
    K3_CHECK_NEAR(renderMiddleRow(slantedSun(true), noCaster)[SHADOWED], SUN, 1e-4f);
}

K3_TEST(shadow_alpha_tested_caster_lets_light_through_its_holes)
{
    // Fully transparent texture: the alpha-tested occluder casts nothing.
    auto mask = std::make_shared<k3::Material>();
    mask->alphaMode = k3::AlphaMode::Mask;
    mask->diffuseMap = std::make_shared<k3::Texture>(k3::Image(4, 4, {1.f, 1.f, 1.f, 0.f}));

    K3_CHECK_NEAR(renderMiddleRow(slantedSun(true), mask)[SHADOWED], SUN, 1e-4f);
}

K3_TEST(shadow_blended_geometry_casts_nothing)
{
    auto glass = std::make_shared<k3::Material>();
    glass->alphaMode = k3::AlphaMode::Blend;
    glass->diffuse.a = 0.5f;

    K3_CHECK_NEAR(renderMiddleRow(slantedSun(true), glass)[SHADOWED], SUN, 1e-4f);
}

K3_TEST(shadow_lit_surfaces_have_no_acne)
{
    // A floor alone must not shadow itself, even under a grazing light.
    auto environment = slantedSun(true);
    environment.lights[0].direction = {3.f, -1.f, 0.f};

    const float expected = 1.f / std::sqrt(10.f);
    for (float value : renderMiddleRow(environment, nullptr))
        K3_REQUIRE(std::abs(value - expected) < 1e-4f);
}

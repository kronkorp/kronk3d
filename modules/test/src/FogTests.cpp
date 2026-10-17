#include "Test.hpp"
#include "render/software/Shading.hpp"
#include <kronk3d/render/software/SoftwareRasterizer.hpp>
#include <cmath>

namespace
{

    using k3::Math::Vector3f;

    // Visibility by brute force: exp(-sum of the density over small steps along the ray).
    float marchedVisibility(const k3::Fog& fog, const Vector3f& from, const Vector3f& to)
    {
        constexpr int STEPS = 40000;
        const Vector3f ray = to - from;
        const float length = Vector3f::length(ray);
        double depth = 0.0;

        for (int i = 0; i < STEPS; ++i) {
            const float y = from.y + ray.y * (static_cast<float>(i) + 0.5f) / STEPS;

            depth += fog.density * std::exp(-fog.heightFalloff * (y - fog.baseHeight)) * length / STEPS;
        }
        return static_cast<float>(std::exp(-depth));
    }

    float visibility(const k3::Fog& fog, const Vector3f& from, const Vector3f& to)
    {
        k3::Camera camera;
        k3::Environment environment;

        camera.position = from;
        environment.fog = fog;

        const auto context = k3::sw::ShadingContext::prepare(camera, environment);

        return k3::sw::fogVisibility(context.fog, context.cameraPosition, to);
    }

    k3::Fog layer(float density, float falloff, float base)
    {
        k3::Fog fog;

        fog.density = density;
        fog.heightFalloff = falloff;
        fog.baseHeight = base;
        return fog;
    }

    constexpr std::uint32_t SIZE = 16;

    // A quad bigger than the view, at z = 0: 5 units in front of the default camera (which sits at z = 5).
    std::shared_ptr<k3::Mesh> wall(float z = 0.f)
    {
        return std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{-20.f, -20.f, z}, {20.f, -20.f, z}, {20.f, 20.f, z}, {-20.f, 20.f, z}},
            .indices = {0, 1, 2, 0, 2, 3},
        });
    }

    std::vector<k3::Math::Color> render(const k3::Environment& environment, std::shared_ptr<k3::Material> material)
    {
        k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

        rasterizer.beginFrame({}, environment);
        rasterizer.draw(wall(), material, k3::Math::Matrix4::identity());
        rasterizer.endFrame();
        return rasterizer.colorBuffer();
    }

    std::shared_ptr<k3::Material> unlitRed()
    {
        auto material = std::make_shared<k3::Material>();

        material->diffuse = k3::Math::Color::Red;
        material->unlit = true;
        return material;
    }

    const k3::Math::Color& middle(const std::vector<k3::Math::Color>& frame)
    {
        return frame[SIZE / 2 * SIZE + SIZE / 2];
    }

}

K3_TEST(fog_uniform_density_follows_beers_law)
{
    K3_CHECK_NEAR(visibility(layer(0.1f, 0.f, 0.f), {0.f, 0.f, 0.f}, {0.f, 0.f, -10.f}), std::exp(-1.f), 1e-5f);
    K3_CHECK_NEAR(visibility(layer(0.1f, 0.f, 0.f), {1.f, 2.f, 3.f}, {1.f, 2.f, 3.f}), 1.f, 1e-6f);      // Nothing in between
    K3_CHECK_NEAR(visibility(layer(0.f, 0.f, 0.f), {0.f, 0.f, 0.f}, {50.f, 0.f, 0.f}), 1.f, 1e-6f);      // No density
}

K3_TEST(fog_height_falloff_matches_a_numerical_integration)
{
    struct Case
    {
        k3::Fog fog;
        Vector3f from, to;
    };
    const Case cases[] = {
        {layer(0.2f, 0.5f, 0.f), {0.f, 0.f, 0.f}, {10.f, 4.f, 3.f}},            // Looking up out of the layer
        {layer(0.2f, 0.5f, 0.f), {0.f, 8.f, 0.f}, {10.f, -2.f, 3.f}},           // From above, down into it
        {layer(0.05f, 0.3f, 5.f), {-3.f, 2.f, 1.f}, {20.f, 2.f, -9.f}},         // Level ray, camera below the base
        {layer(0.4f, 1.5f, -1.f), {0.f, -1.f, 0.f}, {2.f, -3.f, -2.f}},         // Dense, steep, going down
        {layer(0.1f, 0.2f, 0.f), {0.f, 3.f, 0.f}, {0.0001f, 3.0001f, -30.f}},   // Almost level: the series branch
    };

    for (const Case& c : cases)
        K3_CHECK_NEAR(visibility(c.fog, c.from, c.to), marchedVisibility(c.fog, c.from, c.to), 2e-4f);
}

K3_TEST(fog_stays_finite_far_below_its_base)
{
    // exp(falloff * 5000) would overflow a float.
    const float v = visibility(layer(0.1f, 1.f, 0.f), {0.f, -5000.f, 0.f}, {10.f, -5000.f, 0.f});

    K3_CHECK(std::isfinite(v) && v >= 0.f && v <= 1.f);
}

K3_TEST(fog_blends_surfaces_toward_its_color)
{
    k3::Environment environment;

    environment.clearColor = k3::Math::Color::Black;
    environment.fog.color = k3::Math::Color::Blue;
    environment.fog.density = 0.2f;

    const float kept = std::exp(-0.2f * 5.f);       // The wall is 5 units ahead
    const auto c = middle(render(environment, unlitRed()));

    K3_CHECK_NEAR(c.r, kept, 0.01f);
    K3_CHECK_NEAR(c.b, 1.f - kept, 0.01f);
    K3_CHECK_NEAR(c.g, 0.f, 1e-5f);
}

K3_TEST(fog_applies_to_lit_and_transparent_surfaces_and_not_to_the_background)
{
    k3::Environment environment;

    environment.clearColor = {0.f, 1.f, 0.f, 1.f};
    environment.ambient = k3::Math::Color::White;
    environment.fog.color = k3::Math::Color::Blue;
    environment.fog.density = 100.f;                // Opaque fog

    auto lit = std::make_shared<k3::Material>();
    const auto frame = render(environment, lit);

    K3_CHECK_NEAR(middle(frame).b, 1.f, 1e-4f);     // Lit white wall: all fog
    K3_CHECK_NEAR(middle(frame).r, 0.f, 1e-4f);

    auto glass = std::make_shared<k3::Material>();
    glass->unlit = true;
    glass->diffuse = {1.f, 0.f, 0.f, 0.5f};
    glass->alphaMode = k3::AlphaMode::Blend;

    const auto blended = render(environment, glass);

    K3_CHECK_NEAR(middle(blended).b, 0.5f, 1e-3f);          // Fog color at half opacity over the background
    K3_CHECK_NEAR(middle(blended).g, 0.5f, 1e-3f);
    K3_CHECK_NEAR(middle(blended).r, 0.f, 1e-3f);

    // Where nothing is drawn, the background is as it was.
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
    rasterizer.beginFrame({}, environment);
    rasterizer.endFrame();
    K3_CHECK_NEAR(rasterizer.colorBuffer()[0].g, 1.f, 1e-6f);
}

K3_TEST(fog_off_by_default_changes_nothing)
{
    k3::Environment environment;

    environment.clearColor = k3::Math::Color::Black;

    const auto c = middle(render(environment, unlitRed()));

    K3_CHECK_NEAR(c.r, 1.f, 1e-6f);
    K3_CHECK_NEAR(c.b, 0.f, 1e-6f);
}

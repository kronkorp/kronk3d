#include "Test.hpp"
#include "render/ToneMap.hpp"
#include <kronk3d/render/software/SoftwareRasterizer.hpp>

namespace
{

    constexpr std::uint32_t SIZE = 16;

    k3::Camera orthoCamera()
    {
        k3::Camera camera;

        camera.position = {0.f, 0.f, 1.f};
        camera.projection = k3::Projection::Orthographic;
        camera.orthoHeight = 2.f;
        camera.nearPlane = 0.1f;
        camera.farPlane = 10.f;
        return camera;
    }

    std::shared_ptr<k3::Mesh> quad(float half = 5.f)
    {
        return std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{-half, -half, 0.f}, {half, -half, 0.f}, {half, half, 0.f}, {-half, half, 0.f}},
            .normals = {{0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}},
            .indices = {0, 1, 2, 0, 2, 3},
        });
    }

    std::shared_ptr<k3::Material> unlit(const k3::Math::Color& color)
    {
        auto material = std::make_shared<k3::Material>();

        material->diffuse = color;
        material->unlit = true;
        return material;
    }

    k3::Math::Color middle(const k3::Environment& environment, const std::shared_ptr<k3::Material>& material, k3::AntiAliasing mode = k3::AntiAliasing::None)
    {
        k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

        rasterizer.setAntiAliasing(mode);
        rasterizer.beginFrame(orthoCamera(), environment);
        rasterizer.draw(quad(), material, k3::Math::Matrix4::identity());
        rasterizer.endFrame();
        return rasterizer.colorBuffer()[SIZE / 2 * SIZE + SIZE / 2];
    }

    k3::Environment with(k3::ToneMapping mode, float exposure = 1.f)
    {
        k3::Environment environment;

        environment.clearColor = k3::Math::Color::Black;
        environment.toneMapping = mode;
        environment.exposure = exposure;
        return environment;
    }

}

K3_TEST(tone_curves_map_any_value_to_the_unit_range)
{
    using k3::ToneMapping;

    K3_CHECK_NEAR(k3::toneMap(ToneMapping::Reinhard, 4.f, 1.f), 0.8f, 1e-6f);
    K3_CHECK_NEAR(k3::toneMap(ToneMapping::Reinhard, 1.f, 1.f), 0.5f, 1e-6f);
    K3_CHECK_NEAR(k3::toneMap(ToneMapping::Reinhard, 1.f, 3.f), 0.75f, 1e-6f);                    // Exposure first
    K3_CHECK_NEAR(k3::toneMap(ToneMapping::ACES, 0.f, 1.f), 0.f, 1e-6f);
    K3_CHECK_NEAR(k3::toneMap(ToneMapping::ACES, 1.f, 1.f), 0.8038f, 1e-3f);
    K3_CHECK(k3::toneMap(ToneMapping::ACES, 100.f, 1.f) <= 1.f);
    K3_CHECK_NEAR(k3::toneMap(ToneMapping::None, 4.f, 1.f), 1.f, 1e-6f);                          // None clamps
    K3_CHECK_NEAR(k3::toneMap(ToneMapping::None, -1.f, 1.f), 0.f, 1e-6f);

    float previous = -1.f;
    for (float x = 0.f; x < 20.f; x += 0.25f) {                                                    // Monotonic
        const float v = k3::toneMap(ToneMapping::ACES, x, 1.f);

        K3_CHECK(v >= previous);
        previous = v;
    }
}

K3_TEST(tone_mapping_keeps_the_detail_of_bright_surfaces)
{
    const auto bright = unlit({4.f, 2.f, 1.f, 1.f});

    // Without tone mapping the colors above 1 clamp: red and green both saturate.
    const auto clamped = middle(with(k3::ToneMapping::None), bright);
    K3_CHECK(clamped.r == 1.f && clamped.g == 1.f && clamped.b == 1.f);

    const auto reinhard = middle(with(k3::ToneMapping::Reinhard), bright);
    K3_CHECK_NEAR(reinhard.r, 0.8f, 1e-5f);
    K3_CHECK_NEAR(reinhard.g, 2.f / 3.f, 1e-5f);
    K3_CHECK_NEAR(reinhard.b, 0.5f, 1e-5f);

    const auto dimmer = middle(with(k3::ToneMapping::Reinhard, 0.5f), bright);                     // Exposure
    K3_CHECK_NEAR(dimmer.r, 2.f / 3.f, 1e-5f);
}

K3_TEST(a_bright_sun_keeps_its_gradation_with_tone_mapping_only)
{
    auto lit = [](float intensity, k3::ToneMapping mode) {
        k3::Environment environment = with(mode);

        environment.ambient = k3::Math::Color::Black;
        environment.lights = {k3::Light::directional({0.f, 0.f, -1.f}, k3::Math::Color::White, intensity)};
        return middle(environment, std::make_shared<k3::Material>()).r;
    };

    K3_CHECK(lit(4.f, k3::ToneMapping::None) == 1.f && lit(8.f, k3::ToneMapping::None) == 1.f);    // Both clamped: the same
    K3_CHECK(lit(8.f, k3::ToneMapping::ACES) > lit(4.f, k3::ToneMapping::ACES) + 0.01f);           // Still different
    K3_CHECK(lit(8.f, k3::ToneMapping::ACES) <= 1.f);
}

K3_TEST(overlay_is_not_tone_mapped_and_anti_aliasing_averages_displayable_colors)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

    rasterizer.setAntiAliasing(k3::AntiAliasing::SSAA);
    rasterizer.beginFrame(orthoCamera(), with(k3::ToneMapping::Reinhard));
    // A very bright quad covering the left half of the pixel column 8 (its right edge is the middle of the pixel).
    rasterizer.draw(std::make_shared<k3::Mesh>(k3::Mesh{
        .positions = {{-5.f, -5.f, 0.f}, {0.0625f, -5.f, 0.f}, {0.0625f, 5.f, 0.f}, {-5.f, 5.f, 0.f}},
        .indices = {0, 1, 2, 0, 2, 3},
    }), unlit({10.f, 10.f, 10.f, 1.f}), k3::Math::Matrix4::identity());
    rasterizer.drawRect({0.f, 0.f, 2.f, 2.f}, k3::Math::Color::White);
    rasterizer.endFrame();

    const auto& frame = rasterizer.colorBuffer();

    K3_CHECK_NEAR(frame[0].r, 1.f, 1e-6f);                                                          // Overlay: exactly white, not 0.5
    K3_CHECK_NEAR(frame[4 * SIZE + 8].r, 0.5f * (10.f / 11.f), 1e-4f);                              // Half the pixel at 0.909, half black
    K3_CHECK_NEAR(frame[4 * SIZE + 4].r, 10.f / 11.f, 1e-5f);
}

K3_TEST(tone_mapping_is_off_by_default)
{
    k3::Environment environment;

    K3_CHECK(environment.toneMapping == k3::ToneMapping::None && environment.exposure == 1.f);
}

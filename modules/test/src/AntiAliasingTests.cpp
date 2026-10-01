#include "Test.hpp"
#include "render/software/SoftwareRasterizer.hpp"
#include <set>

namespace
{

    constexpr std::uint32_t SIZE = 16;      // 16 px over [-1, 1]: 0.125 unit per pixel

    k3::Camera orthoCamera()
    {
        k3::Camera camera;
        camera.position = {0.f, 0.f, 1.f};
        camera.projection = k3::Projection::Orthographic;
        camera.orthoHeight = 2.f;
        return camera;
    }

    std::shared_ptr<k3::Material> red()
    {
        auto material = std::make_shared<k3::Material>();
        material->diffuse = k3::Math::Color::Red;
        material->unlit = true;
        return material;
    }

    k3::Environment black()
    {
        k3::Environment environment;
        environment.clearColor = k3::Math::Color::Black;
        return environment;
    }

    std::vector<k3::Math::Color> render(k3::AntiAliasing mode, std::shared_ptr<k3::Mesh> mesh)
    {
        k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

        rasterizer.setAntiAliasing(mode);
        rasterizer.beginFrame(orthoCamera(), black());
        rasterizer.draw(mesh, red(), k3::Math::Matrix4::identity());
        rasterizer.endFrame();
        return rasterizer.colorBuffer();
    }

    // Triangle whose hypotenuse crosses the view diagonally.
    std::shared_ptr<k3::Mesh> diagonal()
    {
        return std::make_shared<k3::Mesh>(k3::Mesh{.positions = {{-1.f, -1.f, 0.f}, {1.f, -1.f, 0.f}, {-1.f, 0.7f, 0.f}}});
    }

}

K3_TEST(ssaa_averages_covered_samples)
{
    // Right edge at x = 1/16: the middle of pixel column 8 (which spans [0, 0.125]).
    auto halfPixel = std::make_shared<k3::Mesh>(k3::Mesh{
        .positions = {{-1.f, -1.f, 0.f}, {0.0625f, -1.f, 0.f}, {0.0625f, 1.f, 0.f}, {-1.f, 1.f, 0.f}},
        .indices = {0, 1, 2, 0, 2, 3},
    });

    const auto aliased = render(k3::AntiAliasing::None, halfPixel);
    const auto smooth = render(k3::AntiAliasing::SSAA, halfPixel);
    const std::size_t pixel = 4 * SIZE + 8;

    K3_REQUIRE(smooth.size() == SIZE * SIZE);
    K3_CHECK(aliased[pixel].r == 0.f || aliased[pixel].r == 1.f);
    K3_CHECK_NEAR(smooth[pixel].r, 0.5f, 1e-6f);
    K3_CHECK(smooth[pixel - 1].r == 1.f && smooth[pixel + 1].r == 0.f);
}

K3_TEST(fxaa_softens_edges_and_keeps_flat_areas)
{
    const auto aliased = render(k3::AntiAliasing::None, diagonal());
    const auto smooth = render(k3::AntiAliasing::FXAA, diagonal());

    std::size_t aliasedSteps = 0, smoothSteps = 0;
    for (std::size_t i = 0; i < aliased.size(); ++i) {
        aliasedSteps += aliased[i].r > 0.05f && aliased[i].r < 0.95f;
        smoothSteps += smooth[i].r > 0.05f && smooth[i].r < 0.95f;
    }
    K3_CHECK(aliasedSteps == 0);
    K3_CHECK(smoothSteps > 4);

    // Far from the edge: untouched.
    K3_CHECK(smooth[14 * SIZE + 1].r == 1.f);
    K3_CHECK(smooth[1 * SIZE + 14].r == 0.f);
}

K3_TEST(anti_aliasing_keeps_the_output_size)
{
    k3::SoftwareRasterizer rasterizer(40, 30);

    for (auto mode : {k3::AntiAliasing::SSAA, k3::AntiAliasing::FXAA, k3::AntiAliasing::None, k3::AntiAliasing::SSAA}) {
        rasterizer.setAntiAliasing(mode);
        K3_CHECK(rasterizer.antiAliasing() == mode);
        rasterizer.beginFrame(orthoCamera(), black());
        rasterizer.draw(diagonal(), red(), k3::Math::Matrix4::identity());
        rasterizer.endFrame();

        const k3::Image image = rasterizer.readPixels();
        K3_CHECK(rasterizer.width() == 40 && rasterizer.height() == 30);
        K3_CHECK(image.width == 40 && image.height == 30 && rasterizer.colorBuffer().size() == 40 * 30);
    }
}

K3_TEST(factory_applies_the_anti_aliasing_setting)
{
    auto rasterizer = k3::createRasterizer(k3::Backend::Software, {.width = 8, .height = 8, .antiAliasing = k3::AntiAliasing::FXAA});

    K3_REQUIRE(rasterizer);
    K3_CHECK((*rasterizer)->antiAliasing() == k3::AntiAliasing::FXAA);
}

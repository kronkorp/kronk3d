#include "Test.hpp"
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

    k3::Environment blackEnvironment()
    {
        k3::Environment environment;

        environment.clearColor = k3::Math::Color::Black;
        return environment;
    }

    std::shared_ptr<k3::Material> red()
    {
        auto material = std::make_shared<k3::Material>();

        material->diffuse = k3::Math::Color::Red;
        material->unlit = true;
        return material;
    }

    // A triangle covering the lower left half of the view.
    std::shared_ptr<k3::Mesh> triangle()
    {
        return std::make_shared<k3::Mesh>(k3::Mesh{.positions = {{-1.f, -1.f, 0.f}, {1.f, -1.f, 0.f}, {-1.f, 1.f, 0.f}}});
    }

    // Bigger than any view of these tests.
    std::shared_ptr<k3::Mesh> fullQuad()
    {
        return std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{-9.f, -9.f, 0.f}, {9.f, -9.f, 0.f}, {9.f, 9.f, 0.f}, {-9.f, 9.f, 0.f}},
            .indices = {0, 1, 2, 0, 2, 3},
        });
    }

    // True when every `block` x `block` square of the frame is of a single color.
    bool blocksAreUniform(const std::vector<k3::Math::Color>& frame, std::uint32_t width, std::uint32_t height, std::uint32_t block)
    {
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                const auto& first = frame[(y / block * block) * width + x / block * block];
                const auto& pixel = frame[y * width + x];

                if (pixel.r != first.r || pixel.g != first.g || pixel.b != first.b)
                    return false;
            }
        }
        return true;
    }

}

K3_TEST(render_scale_enlarges_the_pixels_of_the_scene)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

    rasterizer.setRenderScale(0.25f);
    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(triangle(), red(), k3::Math::Matrix4::identity());
    rasterizer.endFrame();

    K3_CHECK(rasterizer.width() == SIZE && rasterizer.height() == SIZE);        // The output size does not change
    K3_REQUIRE(rasterizer.colorBuffer().size() == SIZE * SIZE);
    K3_CHECK(blocksAreUniform(rasterizer.colorBuffer(), SIZE, SIZE, 4));
    K3_CHECK(rasterizer.colorBuffer()[15 * SIZE + 0].r == 1.f);                 // Lower left: inside
    K3_CHECK(rasterizer.colorBuffer()[0 * SIZE + 15].r == 0.f);                 // Upper right: outside
    K3_CHECK(rasterizer.readPixels().width == SIZE && rasterizer.readPixels().height == SIZE);

    // Without scaling the same triangle has an edge that blocks of 4 cannot follow.
    rasterizer.setRenderScale(1.f);
    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(triangle(), red(), k3::Math::Matrix4::identity());
    rasterizer.endFrame();
    K3_CHECK(!blocksAreUniform(rasterizer.colorBuffer(), SIZE, SIZE, 4));
}

K3_TEST(render_scale_leaves_the_overlay_at_full_resolution)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

    rasterizer.setRenderScale(0.25f);
    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(fullQuad(), red(), k3::Math::Matrix4::identity());
    rasterizer.drawRect({5.f, 5.f, 2.f, 2.f}, k3::Math::Color::White);
    rasterizer.endFrame();

    const auto& frame = rasterizer.colorBuffer();

    K3_CHECK(frame[5 * SIZE + 5].g == 1.f && frame[6 * SIZE + 6].g == 1.f);     // 2 x 2 pixels
    K3_CHECK(frame[4 * SIZE + 4].g == 0.f && frame[7 * SIZE + 7].g == 0.f);     // And nothing around it: not 4 x 4
    K3_CHECK(frame[5 * SIZE + 7].g == 0.f);
}

K3_TEST(render_scale_is_clamped_and_one_changes_nothing)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

    K3_CHECK(rasterizer.renderScale() == 1.f);
    rasterizer.setRenderScale(5.f);
    K3_CHECK(rasterizer.renderScale() == 1.f);
    rasterizer.setRenderScale(0.f);
    K3_CHECK_NEAR(rasterizer.renderScale(), 0.05f, 1e-6f);

    auto render = [](float scale) {
        k3::SoftwareRasterizer r(SIZE, SIZE);

        r.setRenderScale(scale);
        r.beginFrame(orthoCamera(), blackEnvironment());
        r.draw(triangle(), red(), k3::Math::Matrix4::identity());
        r.endFrame();
        return r.colorBuffer();
    };
    const auto plain = render(1.f);
    const auto same = render(1.f);

    for (std::size_t i = 0; i < plain.size(); ++i)
        K3_REQUIRE(plain[i].r == same[i].r);

    // A tiny scale still renders: one scene pixel for the whole view.
    const auto tiny = render(0.05f);
    K3_CHECK(blocksAreUniform(tiny, SIZE, SIZE, SIZE));
}

K3_TEST(render_scale_works_with_anti_aliasing_and_odd_sizes)
{
    for (auto mode : {k3::AntiAliasing::None, k3::AntiAliasing::FXAA, k3::AntiAliasing::SSAA}) {
        k3::SoftwareRasterizer rasterizer(20, 10);

        rasterizer.setAntiAliasing(mode);
        rasterizer.setRenderScale(0.3f);                            // 6 x 3 scene pixels for 20 x 10
        rasterizer.beginFrame(orthoCamera(), blackEnvironment());
        rasterizer.draw(fullQuad(), red(), k3::Math::Matrix4::identity());
        rasterizer.endFrame();

        K3_REQUIRE(rasterizer.colorBuffer().size() == 200);
        for (const auto& pixel : rasterizer.colorBuffer())
            K3_REQUIRE(pixel.r > 0.99f && pixel.g < 0.01f);         // The whole output is covered, edges included
        K3_CHECK(rasterizer.readPixels().width == 20 && rasterizer.readPixels().height == 10);
    }
}

K3_TEST(render_scale_goes_through_the_factory_config)
{
    auto created = k3::createRasterizer(k3::Backend::Software, {.width = 8, .height = 8, .renderScale = 0.5f});

    K3_REQUIRE(created);
    K3_CHECK_NEAR((*created)->renderScale(), 0.5f, 1e-6f);
}

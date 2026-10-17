#include "Test.hpp"
#include <kronk3d/render/software/SoftwareRasterizer.hpp>

namespace
{

    constexpr std::uint32_t SIZE = 8;

    k3::Environment black()
    {
        k3::Environment environment;
        environment.clearColor = k3::Math::Color::Black;
        return environment;
    }

    // An empty 3D frame (black), then whatever `overlay` records.
    template <typename Overlay>
    std::vector<k3::Math::Color> render(Overlay overlay, k3::AntiAliasing mode = k3::AntiAliasing::None)
    {
        k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

        rasterizer.setAntiAliasing(mode);
        rasterizer.beginFrame({}, black());
        overlay(rasterizer);
        rasterizer.endFrame();
        return rasterizer.colorBuffer();
    }

    const k3::Math::Color& at(const std::vector<k3::Math::Color>& frame, std::uint32_t x, std::uint32_t y)
    {
        return frame[y * SIZE + x];
    }

    // Two texels, red then green, shown without filtering.
    std::shared_ptr<k3::Texture> redGreen()
    {
        k3::Image image(2, 1);

        image.texel(0, 0)[0] = 255;
        image.texel(0, 0)[3] = 255;
        image.texel(1, 0)[1] = 255;
        image.texel(1, 0)[3] = 255;

        auto texture = std::make_shared<k3::Texture>(image, k3::ColorSpace::Linear);
        texture->sampler.filter = k3::TextureFilter::Nearest;
        return texture;
    }

}

K3_TEST(overlay_rect_covers_the_pixels_it_contains)
{
    const auto frame = render([](k3::IRasterizer& r) { r.drawRect({2.f, 1.f, 3.f, 2.f}, k3::Math::Color::Red); });

    for (std::uint32_t y = 0; y < SIZE; ++y) {
        for (std::uint32_t x = 0; x < SIZE; ++x) {
            const bool inside = x >= 2 && x < 5 && y >= 1 && y < 3;      // Origin top-left, y down

            K3_CHECK(at(frame, x, y).r == (inside ? 1.f : 0.f));
        }
    }
}

K3_TEST(overlay_blends_in_submission_order)
{
    const auto frame = render([](k3::IRasterizer& r) {
        r.drawRect({0.f, 0.f, 4.f, 4.f}, k3::Math::Color::Green);
        r.drawRect({2.f, 2.f, 4.f, 4.f}, {1.f, 0.f, 0.f, 0.5f});
    });

    K3_CHECK_NEAR(at(frame, 0, 0).g, 1.f, 1e-6f);                       // Green only
    K3_CHECK_NEAR(at(frame, 3, 3).r, 0.5f, 1e-6f);                      // Half red over green
    K3_CHECK_NEAR(at(frame, 3, 3).g, 0.5f, 1e-6f);
    K3_CHECK_NEAR(at(frame, 5, 5).r, 0.5f, 1e-6f);                      // Half red over black
    K3_CHECK_NEAR(at(frame, 5, 5).g, 0.f, 1e-6f);
}

K3_TEST(overlay_image_shows_all_of_the_texture_or_the_source_rect)
{
    const auto texture = redGreen();
    const auto whole = render([&](k3::IRasterizer& r) { r.drawImage(texture, {0.f, 0.f, 4.f, 2.f}); });
    const auto second = render([&](k3::IRasterizer& r) { r.drawImage(texture, {0.f, 0.f, 4.f, 2.f}, {1.f, 0.f, 1.f, 1.f}); });
    const auto flipped = render([&](k3::IRasterizer& r) { r.drawImage(texture, {0.f, 0.f, 4.f, 2.f}, {2.f, 0.f, -2.f, 1.f}); });

    K3_CHECK(at(whole, 1, 0).r == 1.f && at(whole, 1, 0).g == 0.f);
    K3_CHECK(at(whole, 2, 1).g == 1.f && at(whole, 2, 1).r == 0.f);
    K3_CHECK(at(second, 0, 0).g == 1.f && at(second, 3, 1).g == 1.f);
    K3_CHECK(at(flipped, 1, 0).g == 1.f && at(flipped, 2, 0).r == 1.f);
    K3_CHECK(at(whole, 4, 0).r == 0.f && at(whole, 0, 2).g == 0.f);     // Outside the destination
}

K3_TEST(overlay_tint_multiplies_the_texture)
{
    const auto frame = render([](k3::IRasterizer& r) { r.drawImage(redGreen(), {0.f, 0.f, 4.f, 2.f}, {}, {0.5f, 0.5f, 0.5f, 1.f}); });

    K3_CHECK_NEAR(at(frame, 0, 0).r, 0.5f, 1e-6f);
    K3_CHECK_NEAR(at(frame, 3, 0).g, 0.5f, 1e-6f);
}

K3_TEST(overlay_is_clipped_and_ignores_empty_rectangles)
{
    const auto frame = render([](k3::IRasterizer& r) {
        r.drawRect({-3.f, -3.f, 5.f, 5.f}, k3::Math::Color::Red);          // Covers (0..1, 0..1)
        r.drawRect({6.f, 6.f, 10.f, 10.f}, k3::Math::Color::Green);        // Covers (6..7, 6..7)
        r.drawRect({20.f, 20.f, 4.f, 4.f}, k3::Math::Color::Blue);         // Out
        r.drawRect({3.f, 3.f, 0.f, 4.f}, k3::Math::Color::Blue);           // Empty
        r.drawRect({3.f, 3.f, -2.f, 4.f}, k3::Math::Color::Blue);          // Negative
    });

    K3_CHECK(at(frame, 1, 1).r == 1.f && at(frame, 2, 2).r == 0.f);
    K3_CHECK(at(frame, 7, 7).g == 1.f && at(frame, 5, 5).g == 0.f);
    for (const auto& pixel : frame)
        K3_REQUIRE(pixel.b == 0.f);
}

K3_TEST(overlay_is_in_output_pixels_and_not_filtered_by_anti_aliasing)
{
    for (auto mode : {k3::AntiAliasing::SSAA, k3::AntiAliasing::FXAA}) {
        const auto frame = render([](k3::IRasterizer& r) { r.drawRect({0.f, 0.f, 2.f, 2.f}, k3::Math::Color::Red); }, mode);

        for (std::uint32_t y = 0; y < SIZE; ++y)
            for (std::uint32_t x = 0; x < SIZE; ++x)
                K3_REQUIRE(at(frame, x, y).r == ((x < 2 && y < 2) ? 1.f : 0.f));       // Hard edges
    }
}

K3_TEST(overlay_is_forgotten_after_the_frame)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

    rasterizer.beginFrame({}, black());
    rasterizer.drawRect({0.f, 0.f, 8.f, 8.f}, k3::Math::Color::Red);
    rasterizer.endFrame();
    rasterizer.beginFrame({}, black());
    rasterizer.endFrame();

    K3_CHECK(rasterizer.colorBuffer()[0].r == 0.f);
}

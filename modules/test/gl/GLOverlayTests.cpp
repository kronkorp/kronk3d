#include "GLFixture.hpp"

using namespace k3gl;

K3_TEST(gl_overlay_matches_the_software_backend)
{
    if (!openGLAvailable())
        return;

    constexpr std::uint32_t W = 32, H = 24;
    auto gpu = hardware(W, H);
    K3_REQUIRE(gpu);
    k3::SoftwareRasterizer cpu(W, H);

    k3::Image image(2, 2);
    const std::uint8_t texels[4][4] = {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 128}, {255, 255, 255, 255}};
    for (std::uint32_t i = 0; i < 4; ++i)
        for (int c = 0; c < 4; ++c)
            image.texel(i % 2, i / 2)[c] = texels[i][c];
    auto sprite = std::make_shared<k3::Texture>(image);
    sprite->sampler.filter = k3::TextureFilter::Nearest;

    k3::Environment environment;
    environment.clearColor = {0.1f, 0.2f, 0.3f, 1.f};

    auto draw = [&](k3::IRasterizer& rasterizer) {
        rasterizer.beginFrame({}, environment);
        rasterizer.drawRect({2.f, 2.f, 12.f, 8.f}, {1.f, 0.5f, 0.f, 0.5f});
        rasterizer.drawImage(sprite, {6.f, 4.f, 16.f, 16.f});
        rasterizer.drawImage(sprite, {-4.f, 14.f, 12.f, 12.f}, {1.f, 0.f, -1.f, 2.f}, {1.f, 1.f, 1.f, 0.75f});    // Clipped, flipped
        rasterizer.endFrame();
    };

    for (auto mode : {k3::AntiAliasing::None, k3::AntiAliasing::FXAA, k3::AntiAliasing::SSAA}) {
        gpu->setAntiAliasing(mode);
        cpu.setAntiAliasing(mode);
        draw(*gpu);
        draw(cpu);

        const auto a = gpu->readPixels().pixels;
        const auto b = cpu.readPixels().pixels;
        K3_REQUIRE(a.size() == b.size());

        int worst = 0;
        for (std::size_t i = 0; i < a.size(); ++i)
            worst = std::max(worst, std::abs(int(a[i]) - int(b[i])));
        std::cout << "    anti-aliasing " << static_cast<int>(mode) << ": worst channel difference " << worst << std::endl;
        K3_CHECK(worst <= 2);       // Only the sRGB rounding of the blend may differ
    }
}

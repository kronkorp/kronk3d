#include "GLFixture.hpp"

using namespace k3gl;

K3_TEST(gl_tone_mapping_matches_the_software_backend_and_can_be_toggled)
{
    if (!openGLAvailable())
        return;

    constexpr std::uint32_t W = 200, H = 150;
    auto gpu = hardware(W, H);
    K3_REQUIRE(gpu);
    k3::SoftwareRasterizer cpu(W, H);

    auto floor = std::make_shared<k3::Material>();
    floor->diffuseMap = checker(90, 220);
    auto glow = std::make_shared<k3::Material>();
    glow->diffuse = {0.2f, 0.2f, 0.2f, 1.f};
    glow->emissive = {3.f, 1.5f, 0.5f, 1.f};                    // Far above 1
    auto glass = std::make_shared<k3::Material>();
    glass->diffuse = {2.f, 3.f, 5.f, 0.5f};                     // Bright and translucent
    glass->unlit = true;
    glass->alphaMode = k3::AlphaMode::Blend;
    auto ball = sphere(16, 32);

    auto draw = [&](k3::IRasterizer& rasterizer, k3::ToneMapping mode, float exposure) {
        k3::Camera camera;
        camera.position = {0.f, 1.5f, 4.f};
        camera.lookAt({0.f, 0.f, 0.f});

        k3::Environment environment;
        environment.toneMapping = mode;
        environment.exposure = exposure;
        environment.skybox = k3::Skybox::gradient({0.1f, 0.3f, 0.9f, 1.f}, {0.8f, 0.9f, 1.f, 1.f}, {0.2f, 0.2f, 0.2f, 1.f});
        environment.skybox.tint = {2.f, 2.f, 2.f, 1.f};
        environment.lights = {k3::Light::directional({-0.4f, -1.f, -0.5f}, {1.f, 0.95f, 0.9f, 1.f}, 4.f), k3::Light::point({1.f, 1.f, 1.f}, {1.f, 0.6f, 0.3f, 1.f}, 6.f, 8.f)};
        environment.lights[0].castShadows = true;

        rasterizer.beginFrame(camera, environment);
        rasterizer.draw(quad(4.f, -1.f), floor, k3::Math::Matrix4::identity());
        rasterizer.draw(ball, glow, k3::Math::Matrix4::translate({-1.f, 0.f, 0.f}));
        rasterizer.draw(ball, glass, k3::Math::Matrix4::translate({0.9f, 0.f, 0.6f}) * k3::Math::Matrix4::scale(0.7f));
        rasterizer.endFrame();
    };

    // None, then each curve, then None again: the floating-point target is allocated and dropped as the mode changes.
    const std::pair<k3::ToneMapping, float> steps[] = {
        {k3::ToneMapping::None, 1.f}, {k3::ToneMapping::ACES, 1.f}, {k3::ToneMapping::Reinhard, 0.7f}, {k3::ToneMapping::ACES, 1.5f}, {k3::ToneMapping::None, 1.f},
    };

    for (const auto& [mode, exposure] : steps) {
        draw(*gpu, mode, exposure);
        draw(cpu, mode, exposure);

        const auto a = gpu->readPixels().pixels;
        const auto b = cpu.readPixels().pixels;
        K3_REQUIRE(a.size() == b.size());

        double sum = 0.0;
        std::size_t over8 = 0, over32 = 0;
        for (std::size_t i = 0; i < a.size(); i += 4) {
            int worst = 0;
            for (int c = 0; c < 3; ++c)
                worst = std::max(worst, std::abs(int(a[i + c]) - int(b[i + c])));
            sum += worst;
            over8 += worst > 8;
            over32 += worst > 32;
        }
        const double pixels = W * H;
        std::cout << "    tone mapping " << static_cast<int>(mode) << " x" << exposure << ": mean difference " << sum / pixels << ", > 8: " << 100.0 * over8 / pixels
                  << "%, > 32: " << 100.0 * over32 / pixels << "%" << std::endl;

        K3_CHECK(sum / pixels < 1.0);
        K3_CHECK(over8 / pixels < 0.01);
        K3_CHECK(over32 / pixels < 0.002);
    }
}

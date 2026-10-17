#include "GLFixture.hpp"

using namespace k3gl;

K3_TEST(gl_fog_matches_the_software_backend)
{
    if (!openGLAvailable())
        return;

    constexpr std::uint32_t W = 160, H = 120;
    auto gpu = hardware(W, H);
    K3_REQUIRE(gpu);
    k3::SoftwareRasterizer cpu(W, H);

    auto lit = std::make_shared<k3::Material>();
    auto unlit = std::make_shared<k3::Material>();
    unlit->diffuse = {0.9f, 0.3f, 0.2f, 1.f};
    unlit->unlit = true;
    auto glass = std::make_shared<k3::Material>();
    glass->diffuse = {0.2f, 0.9f, 0.4f, 0.5f};
    glass->alphaMode = k3::AlphaMode::Blend;
    auto ball = sphere(16, 32);

    for (const k3::Fog& fog : {k3::Fog{.color = {0.6f, 0.7f, 0.9f, 1.f}, .density = 0.06f, .heightFalloff = 0.4f, .baseHeight = 0.f},
                               k3::Fog{.color = {0.9f, 0.8f, 0.6f, 1.f}, .density = 0.03f}}) {
        auto draw = [&](k3::IRasterizer& rasterizer) {
            k3::Camera camera;
            camera.position = {0.f, 3.f, 6.f};
            camera.lookAt({0.f, 0.f, -6.f});

            k3::Environment environment;
            environment.fog = fog;
            environment.clearColor = fog.color;
            environment.lights = {k3::Light::directional({-0.4f, -1.f, -0.5f})};

            rasterizer.beginFrame(camera, environment);
            rasterizer.draw(quad(12.f, 0.f), lit, k3::Math::Matrix4::identity());
            rasterizer.draw(ball, lit, k3::Math::Matrix4::translate({-2.f, 1.f, -4.f}));
            rasterizer.draw(ball, unlit, k3::Math::Matrix4::translate({2.f, 1.f, -9.f}) * k3::Math::Matrix4::scale(1.5f));
            rasterizer.draw(ball, glass, k3::Math::Matrix4::translate({0.f, 1.f, -1.f}));
            rasterizer.endFrame();
        };
        draw(*gpu);
        draw(cpu);

        const auto a = gpu->readPixels().pixels;
        const auto b = cpu.readPixels().pixels;
        K3_REQUIRE(a.size() == b.size());

        double sum = 0.0;
        std::size_t over8 = 0;
        for (std::size_t i = 0; i < a.size(); i += 4) {
            int worst = 0;
            for (int c = 0; c < 3; ++c)
                worst = std::max(worst, std::abs(int(a[i + c]) - int(b[i + c])));
            sum += worst;
            over8 += worst > 8;
        }
        const double pixels = W * H;
        std::cout << "    fog density " << fog.density << ": mean difference " << sum / pixels << ", > 8: " << 100.0 * over8 / pixels << "%" << std::endl;

        K3_CHECK(sum / pixels < 0.5);
        K3_CHECK(over8 / pixels < 0.005);
    }
}

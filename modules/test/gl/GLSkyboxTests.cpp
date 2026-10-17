#include "GLFixture.hpp"

using namespace k3gl;

K3_TEST(gl_skybox_matches_the_software_backend)
{
    if (!openGLAvailable())
        return;

    constexpr std::uint32_t W = 64, H = 48;
    auto gpu = hardware(W, H);
    K3_REQUIRE(gpu);
    k3::SoftwareRasterizer cpu(W, H);

    // Distinct, non-uniform faces so that a wrong face, flip or rotation shows.
    k3::Skybox sky;
    for (std::size_t face = 0; face < 6; ++face) {
        k3::Image image(32, 32);

        for (std::uint32_t y = 0; y < 32; ++y) {
            for (std::uint32_t x = 0; x < 32; ++x) {
                std::uint8_t* texel = image.texel(x, y);

                texel[0] = static_cast<std::uint8_t>(40 * face + 20);
                texel[1] = static_cast<std::uint8_t>(x * 8);
                texel[2] = static_cast<std::uint8_t>(y * 8);
                texel[3] = 255;
            }
        }
        sky.faces[face] = std::make_shared<k3::Texture>(image);
        sky.faces[face]->sampler.filter = k3::TextureFilter::Nearest;
    }
    sky.tint = {0.9f, 1.f, 0.8f, 1.f};

    auto quad = std::make_shared<k3::Mesh>(k3::Mesh{
        .positions = {{-1.f, -1.f, -3.f}, {0.f, -1.f, -3.f}, {0.f, 1.f, -3.f}, {-1.f, 1.f, -3.f}},
        .indices = {0, 1, 2, 0, 2, 3},
    });
    auto solid = std::make_shared<k3::Material>();
    solid->diffuse = k3::Math::Color::Red;
    solid->unlit = true;
    auto glass = std::make_shared<k3::Material>();
    glass->diffuse = {0.f, 0.f, 1.f, 0.5f};
    glass->unlit = true;
    glass->alphaMode = k3::AlphaMode::Blend;

    for (const k3::Camera& view : {k3::Camera{.yaw = 0.f, .pitch = 0.f}, k3::Camera{.yaw = 2.3f, .pitch = 0.4f}, k3::Camera{.yaw = -0.7f, .pitch = -0.6f}}) {
        for (k3::IRasterizer* rasterizer : {static_cast<k3::IRasterizer*>(gpu.get()), static_cast<k3::IRasterizer*>(&cpu)}) {
            k3::Environment environment;

            environment.skybox = sky;
            rasterizer->beginFrame(view, environment);
            rasterizer->draw(quad, solid, k3::Math::Matrix4::translate({view.yaw == 0.f ? 0.f : 99.f, 0.f, 0.f}));
            rasterizer->draw(quad, glass, k3::Math::Matrix4::translate({0.5f, 0.f, 0.5f}));
            rasterizer->endFrame();
        }

        const auto a = gpu->readPixels().pixels;
        const auto b = cpu.readPixels().pixels;
        K3_REQUIRE(a.size() == b.size());

        double sum = 0.0;
        std::size_t over16 = 0;
        for (std::size_t i = 0; i < a.size(); i += 4) {
            int worst = 0;
            for (int c = 0; c < 3; ++c)
                worst = std::max(worst, std::abs(int(a[i + c]) - int(b[i + c])));
            sum += worst;
            over16 += worst > 16;
        }
        const double pixels = W * H;
        std::cout << "    yaw " << view.yaw << ": mean difference " << sum / pixels << ", > 16: " << 100.0 * over16 / pixels << "%" << std::endl;

        K3_CHECK(sum / pixels < 1.0);
        K3_CHECK(over16 / pixels < 0.01);
    }
}

#include "GLFixture.hpp"

using namespace k3gl;

K3_TEST(gl_flat_shading_and_hemisphere_ambient_match_the_software_backend)
{
    if (!openGLAvailable())
        return;

    constexpr std::uint32_t W = 200, H = 150;
    auto gpu = hardware(W, H);
    K3_REQUIRE(gpu);
    k3::SoftwareRasterizer cpu(W, H);

    auto smooth = std::make_shared<k3::Material>();
    auto faceted = std::make_shared<k3::Material>();
    faceted->flatShading = true;
    auto ball = sphere(10, 16);                 // Few triangles: the facets are big

    auto draw = [&](k3::IRasterizer& rasterizer) {
        k3::Camera camera;
        camera.position = {0.f, 1.5f, 4.f};
        camera.lookAt({0.f, 0.f, 0.f});

        k3::Environment environment;
        environment.ambient = {0.5f, 0.55f, 0.7f, 1.f};
        environment.ambientGround = k3::Math::Color{0.2f, 0.15f, 0.1f, 1.f};
        environment.lights = {k3::Light::directional({-0.4f, -1.f, -0.5f}, {1.f, 0.95f, 0.9f, 1.f}, 1.2f)};

        rasterizer.beginFrame(camera, environment);
        rasterizer.draw(quad(4.f, -1.f), smooth, k3::Math::Matrix4::identity());
        rasterizer.draw(ball, smooth, k3::Math::Matrix4::translate({-1.2f, 0.f, 0.f}));
        rasterizer.draw(ball, faceted, k3::Math::Matrix4::translate({1.2f, 0.f, 0.f}));
        rasterizer.endFrame();
    };
    draw(*gpu);
    draw(cpu);

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
    std::cout << "    flat shading + hemisphere: mean difference " << sum / pixels << ", > 8: " << 100.0 * over8 / pixels << "%, > 32: " << 100.0 * over32 / pixels << "%" << std::endl;

    K3_CHECK(sum / pixels < 1.0);
    K3_CHECK(over8 / pixels < 0.01);
    K3_CHECK(over32 / pixels < 0.002);
}

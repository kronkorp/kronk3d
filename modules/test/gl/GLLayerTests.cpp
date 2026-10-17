#include "GLFixture.hpp"

using namespace k3gl;

K3_TEST(gl_layer_is_drawn_over_nearer_geometry)
{
    if (!openGLAvailable())
        return;

    auto gpu = hardware(8, 8);
    K3_REQUIRE(gpu);

    k3::Camera camera;
    camera.position = {0.f, 0.f, 1.f};
    camera.projection = k3::Projection::Orthographic;
    camera.orthoHeight = 2.f;
    auto quadAt = [](float z) {
        return std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{-1.f, -1.f, z}, {1.f, -1.f, z}, {1.f, 1.f, z}, {-1.f, 1.f, z}},
            .indices = {0, 1, 2, 0, 2, 3},
        });
    };
    auto color = [](const k3::Math::Color& c) {
        auto material = std::make_shared<k3::Material>();

        material->diffuse = c;
        material->unlit = true;
        return material;
    };
    k3::Environment environment;
    environment.clearColor = k3::Math::Color::Black;

    gpu->beginFrame(camera, environment);
    gpu->draw(quadAt(0.f), color(k3::Math::Color::Red), k3::Math::Matrix4::identity());
    gpu->draw(quadAt(-5.f), color(k3::Math::Color::Green), k3::Math::Matrix4::identity());
    gpu->endFrame();
    K3_CHECK(gpu->readPixels().texel(4, 4)[0] == 255 && gpu->readPixels().texel(4, 4)[1] == 0);

    gpu->beginFrame(camera, environment);
    gpu->draw(quadAt(0.f), color(k3::Math::Color::Red), k3::Math::Matrix4::identity());
    gpu->beginLayer(camera);
    gpu->draw(quadAt(-5.f), color(k3::Math::Color::Green), k3::Math::Matrix4::identity());
    gpu->endFrame();
    K3_CHECK(gpu->readPixels().texel(4, 4)[1] == 255 && gpu->readPixels().texel(4, 4)[0] == 0);
}

K3_TEST(gl_layers_match_the_software_backend)
{
    if (!openGLAvailable())
        return;

    constexpr std::uint32_t W = 160, H = 120;
    auto gpu = hardware(W, H);
    K3_REQUIRE(gpu);
    k3::SoftwareRasterizer cpu(W, H);

    // The first layer: a sky, fog, a sun with shadows, a lit floor and a ball. The second: a lit box held
    // close to the camera, with a narrower field of view and its own near plane.
    k3::Environment environment;
    environment.skybox = k3::Skybox::gradient({0.1f, 0.3f, 0.8f, 1.f}, {0.7f, 0.8f, 0.9f, 1.f}, {0.2f, 0.2f, 0.2f, 1.f});
    environment.fog = {.color = {0.7f, 0.8f, 0.9f, 1.f}, .density = 0.03f, .heightFalloff = 0.2f};
    environment.lights = {k3::Light::directional({-0.4f, -1.f, -0.5f})};
    environment.lights[0].castShadows = true;

    k3::Camera camera;
    camera.position = {0.f, 2.f, 6.f};
    camera.lookAt({0.f, 0.f, 0.f});
    k3::Camera weapon = camera;
    weapon.fovY = 0.6f;
    weapon.nearPlane = 0.02f;

    auto lit = std::make_shared<k3::Material>();
    auto metal = std::make_shared<k3::Material>();
    metal->diffuse = {0.8f, 0.5f, 0.2f, 1.f};
    metal->specular = {0.7f, 0.7f, 0.7f, 1.f};
    auto ball = sphere(16, 32);
    auto held = sphere(12, 24);

    auto draw = [&](k3::IRasterizer& rasterizer) {
        rasterizer.beginFrame(camera, environment);
        rasterizer.draw(quad(12.f, 0.f), lit, k3::Math::Matrix4::identity());
        rasterizer.draw(ball, lit, k3::Math::Matrix4::translate({-1.f, 1.f, -2.f}));
        rasterizer.beginLayer(weapon);
        // In front of the camera, a bit to the right and down, in camera space turned into world space.
        const auto placement = k3::Math::Matrix4::translate(camera.position + camera.forward() * 0.6f + camera.right() * 0.25f - camera.up() * 0.15f) * k3::Math::Matrix4::scale(0.12f);
        rasterizer.draw(held, metal, placement);
        rasterizer.endFrame();
    };
    draw(*gpu);
    draw(cpu);

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
    std::cout << "    layers: mean difference " << sum / pixels << ", > 16: " << 100.0 * over16 / pixels << "%" << std::endl;

    K3_CHECK(sum / pixels < 1.0);
    K3_CHECK(over16 / pixels < 0.01);
}

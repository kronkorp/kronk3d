#include "GLFixture.hpp"

using namespace k3gl;

K3_TEST(gl_debug_lines_cover_the_same_pixels_as_the_software_backend)
{
    if (!openGLAvailable())
        return;

    constexpr std::uint32_t N = 16;
    auto gpu = hardware(N, N);
    K3_REQUIRE(gpu);
    k3::SoftwareRasterizer cpu(N, N);

    k3::Camera camera;
    camera.position = {0.f, 0.f, 1.f};
    camera.projection = k3::Projection::Orthographic;
    camera.orthoHeight = 2.f;
    camera.nearPlane = 0.1f;
    camera.farPlane = 10.f;
    k3::Environment environment;
    environment.clearColor = k3::Math::Color::Black;

    auto wall = std::make_shared<k3::Mesh>(k3::Mesh{
        .positions = {{-2.f, -2.f, 0.5f}, {2.f, -2.f, 0.5f}, {2.f, 2.f, 0.5f}, {-2.f, 2.f, 0.5f}},
        .indices = {0, 1, 2, 0, 2, 3},
    });
    auto green = std::make_shared<k3::Material>();
    green->diffuse = k3::Math::Color::Green;
    green->unlit = true;

    auto wx = [](float pixel) { return pixel / 8.f - 1.f; };
    auto wy = [](float pixel) { return 1.f - pixel / 8.f; };
    auto draw = [&](k3::IRasterizer& rasterizer) {
        rasterizer.beginFrame(camera, environment);
        // A wall between the camera (z = 1) and the lines at z = 0; the last two lines are in front of it.
        rasterizer.draw(wall, green, k3::Math::Matrix4::identity());
        rasterizer.drawLine({wx(2.5f), wy(4.5f), 0.f}, {wx(10.5f), wy(4.5f), 0.f}, k3::Math::Color::Red);                 // Behind the wall
        rasterizer.drawLine({wx(2.5f), wy(7.5f), 0.f}, {wx(10.5f), wy(7.5f), 0.f}, k3::Math::Color::Red, false);          // Always visible
        rasterizer.drawLine({wx(12.5f), wy(1.5f), 0.8f}, {wx(12.5f), wy(13.5f), 0.8f}, {1.f, 1.f, 0.f, 1.f});               // Vertical, in front
        rasterizer.drawLine({wx(1.5f), wy(10.5f), 0.8f}, {wx(1.5f), wy(14.5f), 0.8f}, {0.f, 0.f, 1.f, 0.5f}, true);        // Translucent
        rasterizer.endFrame();
    };
    draw(*gpu);
    draw(cpu);

    const auto a = gpu->readPixels().pixels;
    const auto b = cpu.readPixels().pixels;
    K3_REQUIRE(a.size() == b.size());

    int worst = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        worst = std::max(worst, std::abs(int(a[i]) - int(b[i])));
    std::cout << "    debug lines: worst channel difference " << worst << std::endl;
    K3_CHECK(worst <= 2);

    // The depth-tested line behind the wall must really be hidden, on both.
    K3_CHECK(gpu->readPixels().texel(5, 4)[0] == 0);
}

#include "GLFixture.hpp"

using namespace k3gl;

K3_TEST(gl_render_scale_matches_the_software_backend)
{
    if (!openGLAvailable())
        return;

    constexpr std::uint32_t W = 200, H = 150;
    auto gpu = hardware(W, H);
    K3_REQUIRE(gpu);
    k3::SoftwareRasterizer cpu(W, H);

    // 0.35 gives a ratio with no exact tie between two source pixels (0.4 would: 2.5), where implementations may round either way.
    for (const float scale : {0.5f, 0.25f, 0.35f}) {
        gpu->setRenderScale(scale);
        cpu.setRenderScale(scale);
        drawScene(*gpu);
        drawScene(cpu);
        const auto a = gpu->readPixels();
        const auto b = cpu.readPixels();
        K3_REQUIRE(a.width == W && a.height == H && b.width == W && b.height == H);

        double sum = 0.0;
        std::size_t over32 = 0;
        for (std::size_t i = 0; i < a.pixels.size(); i += 4) {
            int worst = 0;
            for (int c = 0; c < 3; ++c)
                worst = std::max(worst, std::abs(int(a.pixels[i + c]) - int(b.pixels[i + c])));
            sum += worst;
            over32 += worst > 32;
        }
        const double pixels = W * H;
        std::cout << "    render scale " << scale << ": mean difference " << sum / pixels << ", > 32: " << 100.0 * over32 / pixels << "%" << std::endl;
        // Blocks magnify the small differences of rasterization rules: looser than at full resolution.
        K3_CHECK(sum / pixels < 2.5);
        K3_CHECK(over32 / pixels < 0.03);
    }
}

K3_TEST(gl_render_scale_makes_blocks_and_keeps_the_overlay_sharp)
{
    if (!openGLAvailable())
        return;

    constexpr std::uint32_t N = 16;
    auto gpu = hardware(N, N);
    K3_REQUIRE(gpu);

    k3::Camera camera;
    camera.position = {0.f, 0.f, 1.f};
    camera.projection = k3::Projection::Orthographic;
    camera.orthoHeight = 2.f;
    k3::Environment environment;
    environment.clearColor = k3::Math::Color::Black;
    auto triangle = std::make_shared<k3::Mesh>(k3::Mesh{.positions = {{-1.f, -1.f, 0.f}, {1.f, -1.f, 0.f}, {-1.f, 1.f, 0.f}}});
    auto red = std::make_shared<k3::Material>();
    red->diffuse = k3::Math::Color::Red;
    red->unlit = true;

    gpu->setRenderScale(0.25f);
    gpu->beginFrame(camera, environment);
    gpu->draw(triangle, red, k3::Math::Matrix4::identity());
    gpu->drawRect({10.f, 2.f, 1.f, 1.f}, k3::Math::Color::White);
    gpu->endFrame();

    const auto image = gpu->readPixels();
    K3_REQUIRE(image.width == N && image.height == N);

    // Blocks of 4 x 4 pixels everywhere except under the overlay's single white pixel.
    for (std::uint32_t y = 0; y < N; ++y) {
        for (std::uint32_t x = 0; x < N; ++x) {
            if (x == 10 && y == 2)
                continue;
            const auto* first = image.texel(x / 4 * 4, y / 4 * 4);
            const auto* pixel = image.texel(x, y);

            K3_REQUIRE(pixel[0] == first[0] && pixel[1] == first[1] && pixel[2] == first[2]);
        }
    }
    K3_CHECK(image.texel(10, 2)[1] == 255);                 // The overlay pixel is white...
    K3_CHECK(image.texel(11, 2)[1] == 0);                   // ...and only one pixel wide
}

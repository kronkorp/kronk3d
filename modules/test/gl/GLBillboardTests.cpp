#include "GLFixture.hpp"

using namespace k3gl;

K3_TEST(gl_billboards_match_the_software_backend_while_their_number_changes)
{
    if (!openGLAvailable())
        return;

    constexpr std::uint32_t W = 160, H = 120;
    auto gpu = hardware(W, H);
    K3_REQUIRE(gpu);
    k3::SoftwareRasterizer cpu(W, H);

    // A 2 x 2 sprite sheet, shown with its own colors and a soft alpha.
    k3::Image image(2, 2);
    const std::uint8_t texels[4][4] = {{255, 80, 40, 255}, {60, 255, 90, 200}, {80, 120, 255, 160}, {255, 255, 255, 255}};
    for (std::uint32_t i = 0; i < 4; ++i)
        for (int c = 0; c < 4; ++c)
            image.texel(i % 2, i / 2)[c] = texels[i][c];
    auto material = std::make_shared<k3::Material>();
    material->unlit = true;
    material->castShadows = false;
    material->alphaMode = k3::AlphaMode::Blend;
    material->diffuseMap = std::make_shared<k3::Texture>(image);
    material->diffuseMap->sampler.filter = k3::TextureFilter::Nearest;

    k3::Camera camera;
    camera.position = {0.f, 1.f, 6.f};
    camera.lookAt({0.f, 0.f, 0.f});
    k3::Environment environment;
    environment.clearColor = {0.1f, 0.15f, 0.2f, 1.f};

    // The same batch type on both rasterizers, with 5, then 2, then 9 quads: the GPU buffer is refreshed in place
    // when the count stays and rebuilt when it changes.
    for (const int count : {5, 2, 9, 9}) {
        k3::BillboardBatch onGpu, onCpu;

        for (int i = 0; i < count; ++i) {
            const float t = static_cast<float>(i);
            const k3::Billboard billboard{
                .position = {-2.f + 0.55f * t, 0.3f * std::sin(t), 0.4f * std::cos(t * 1.7f)},
                .size = {0.9f, 0.9f},
                .color = {1.f, 1.f - 0.07f * t, 1.f, 0.9f},
                .rotation = 0.4f * t,
                .uv = {(i % 2) * 0.5f, ((i / 2) % 2) * 0.5f, (i % 2) * 0.5f + 0.5f, ((i / 2) % 2) * 0.5f + 0.5f},
            };

            onGpu.add(billboard);
            onCpu.add(billboard);
        }
        gpu->beginFrame(camera, environment);
        onGpu.draw(*gpu, camera, material);
        gpu->endFrame();
        cpu.beginFrame(camera, environment);
        onCpu.draw(cpu, camera, material);
        cpu.endFrame();

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
        std::cout << "    " << count << " billboards: mean difference " << sum / pixels << ", > 16: " << 100.0 * over16 / pixels << "%" << std::endl;

        K3_CHECK(sum / pixels < 0.5);
        K3_CHECK(over16 / pixels < 0.005);
    }
}

#include "GLFixture.hpp"

using namespace k3gl;

K3_TEST(gl_matches_the_software_backend)
{
    if (!openGLAvailable())
        return;

    constexpr std::uint32_t W = 200, H = 150;
    auto gpu = hardware(W, H);
    K3_REQUIRE(gpu);
    k3::SoftwareRasterizer cpu(W, H);

    for (auto mode : {k3::AntiAliasing::None, k3::AntiAliasing::FXAA, k3::AntiAliasing::SSAA}) {
        gpu->setAntiAliasing(mode);
        cpu.setAntiAliasing(mode);
        drawScene(*gpu);
        drawScene(cpu);

        const auto a = gpu->readPixels().pixels;
        const auto b = cpu.readPixels().pixels;
        K3_REQUIRE(a.size() == b.size());

        // Rasterization rules and texture LOD differ slightly between implementations: compare
        // statistically, tight enough to catch any real shading divergence.
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
        std::cout << "    anti-aliasing " << static_cast<int>(mode) << ": mean difference " << sum / pixels
                  << ", > 8: " << 100.0 * over8 / pixels << "%, > 32: " << 100.0 * over32 / pixels << "%" << std::endl;

        K3_CHECK(sum / pixels < 1.0);
        K3_CHECK(over8 / pixels < 0.01);
        K3_CHECK(over32 / pixels < 0.002);
    }
}

K3_TEST(gl_read_pixels_returns_the_top_row_first)
{
    if (!openGLAvailable())
        return;

    auto gpu = hardware(8, 8);
    K3_REQUIRE(gpu);

    // Orthographic view of the [-1, 1] square; the red quad covers its upper half only.
    k3::Camera camera;
    camera.position = {0.f, 0.f, 1.f};
    camera.projection = k3::Projection::Orthographic;
    camera.orthoHeight = 2.f;
    k3::Environment environment;
    environment.clearColor = k3::Math::Color::Black;
    auto top = std::make_shared<k3::Mesh>(k3::Mesh{
        .positions = {{-1.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {1.f, 1.f, 0.f}, {-1.f, 1.f, 0.f}},
        .indices = {0, 1, 2, 0, 2, 3},
    });
    auto red = std::make_shared<k3::Material>();
    red->diffuse = k3::Math::Color::Red;
    red->unlit = true;

    gpu->beginFrame(camera, environment);
    gpu->draw(top, red, k3::Math::Matrix4::identity());
    gpu->endFrame();

    const k3::Image image = gpu->readPixels();
    K3_CHECK(image.texel(4, 0)[0] == 255);
    K3_CHECK(image.texel(4, 7)[0] == 0);
}

K3_TEST(gl_resize_changes_the_output_size)
{
    if (!openGLAvailable())
        return;

    auto gpu = hardware(16, 16);
    K3_REQUIRE(gpu);

    gpu->resize(64, 32);
    gpu->beginFrame(k3::Camera{}, k3::Environment{});
    gpu->endFrame();

    const k3::Image image = gpu->readPixels();
    K3_CHECK(image.width == 64 && image.height == 32);
    K3_CHECK(gpu->width() == 64 && gpu->height() == 32);
}

K3_TEST(gl_image_presenter_draws_the_image_upright)
{
    if (!openGLAvailable())
        return;

    auto presenter = k3::ImagePresenter::create(LOADER);
    K3_REQUIRE(presenter);

    // 2x2 target: OpenGL row 0 is the bottom one.
    using namespace k3::gl;
    GLuint texture = 0, framebuffer = 0;
    GenTextures(1, &texture);
    BindTexture(TEXTURE_2D, texture);
    TexImage2D(TEXTURE_2D, 0, static_cast<GLint>(RGBA8), 2, 2, 0, RGBA, UNSIGNED_BYTE, nullptr);
    TexParameteri(TEXTURE_2D, TEXTURE_MIN_FILTER, NEAREST);
    TexParameteri(TEXTURE_2D, TEXTURE_MAX_LEVEL, 0);
    GenFramebuffers(1, &framebuffer);
    BindFramebuffer(FRAMEBUFFER, framebuffer);
    FramebufferTexture2D(FRAMEBUFFER, COLOR_ATTACHMENT0, TEXTURE_2D, texture, 0);
    K3_REQUIRE(CheckFramebufferStatus(FRAMEBUFFER) == FRAMEBUFFER_COMPLETE);

    // Image rows go top-down: red on top, green below.
    k3::Image image(2, 2, k3::Math::Color::Green);
    image.texel(0, 0)[0] = image.texel(1, 0)[0] = 255;
    image.texel(0, 0)[1] = image.texel(1, 0)[1] = 0;
    (*presenter)->present(image, 2, 2);

    std::uint8_t pixels[16] = {};
    PixelStorei(PACK_ALIGNMENT, 1);
    ReadPixels(0, 0, 2, 2, RGBA, UNSIGNED_BYTE, pixels);
    BindFramebuffer(FRAMEBUFFER, 0);
    DeleteFramebuffers(1, &framebuffer);
    DeleteTextures(1, &texture);

    K3_CHECK(pixels[0] == 0 && pixels[1] == 255);      // Bottom row: green
    K3_CHECK(pixels[8] == 255 && pixels[9] == 0);      // Top row: red
}

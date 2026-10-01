#include "Test.hpp"
#include "Kronk3d.hpp"
#include "render/opengl/GL.hpp"
#include <SFML/Window/Context.hpp>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <numbers>

namespace
{

    // One offscreen OpenGL 3.3 core context for the whole run.
    struct GLEnvironment
    {
        std::unique_ptr<sf::Context> context;
        bool available = false;

        GLEnvironment()
        {
#if defined(__linux__)
            // Without a display SFML aborts instead of failing: do not even try.
            if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY"))
                return;
#endif
            const sf::ContextSettings settings(24, 8, 0, 3, 3, sf::ContextSettings::Core);
            context = std::make_unique<sf::Context>(settings, 1, 1);
            context->setActive(true);

            const auto actual = context->getSettings();
            available = actual.majorVersion > 3 || (actual.majorVersion == 3 && actual.minorVersion >= 3);
        }
    };

    GLEnvironment& environment()
    {
        static GLEnvironment instance;
        return instance;
    }

    const k3::GLLoader LOADER = [](const char* name) { return sf::Context::getFunction(name); };

    // False when the test cannot run. That is a failure only when K3_REQUIRE_GL is set (CI).
    bool openGLAvailable()
    {
        if (environment().available)
            return true;
        if (std::getenv("K3_REQUIRE_GL")) {
            k3test::fail(__FILE__, __LINE__, "no OpenGL 3.3 core context (K3_REQUIRE_GL is set)");
            return false;
        }
        std::cout << "    skipped: no OpenGL 3.3 core context" << std::endl;
        return false;
    }

    std::unique_ptr<k3::IRasterizer> hardware(std::uint32_t width, std::uint32_t height)
    {
        auto rasterizer = k3::createRasterizer(k3::Backend::OpenGL, {.width = width, .height = height, .glLoader = LOADER});

        if (!rasterizer) {
            std::cerr << "    " << rasterizer.error() << std::endl;
            return nullptr;
        }
        return std::move(*rasterizer);
    }

    std::shared_ptr<k3::Mesh> sphere(unsigned rings, unsigned segments)
    {
        auto mesh = std::make_shared<k3::Mesh>();
        const float pi = std::numbers::pi_v<float>;

        for (unsigned r = 0; r <= rings; ++r) {
            for (unsigned s = 0; s <= segments; ++s) {
                const float theta = pi * r / rings, phi = 2.f * pi * s / segments;
                const k3::Math::Vector3f n{std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi)};
                mesh->positions.push_back(n);
                mesh->normals.push_back(n);
                mesh->uvs.push_back({static_cast<float>(s) / segments * 4.f, static_cast<float>(r) / rings * 2.f});
            }
        }
        for (unsigned r = 0; r < rings; ++r) {
            for (unsigned s = 0; s < segments; ++s) {
                const std::uint32_t a = r * (segments + 1) + s, b = a + segments + 1;
                mesh->indices.insert(mesh->indices.end(), {a, b, a + 1, a + 1, b, b + 1});
            }
        }
        return mesh;
    }

    std::shared_ptr<k3::Mesh> quad(float half, float y)
    {
        return std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{-half, y, half}, {half, y, half}, {half, y, -half}, {-half, y, -half}},
            .normals = {{0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}},
            .uvs = {{0.f, half}, {half, half}, {half, 0.f}, {0.f, 0.f}},
            .indices = {0, 1, 2, 0, 2, 3},
        });
    }

    std::shared_ptr<k3::Texture> checker(std::uint8_t dark, std::uint8_t light)
    {
        k3::Image image(64, 64);

        for (std::uint32_t y = 0; y < 64; ++y)
            for (std::uint32_t x = 0; x < 64; ++x)
                for (int c = 0; c < 3; ++c)
                    image.texel(x, y)[c] = ((x / 8 + y / 8) % 2) ? light : dark;
        return std::make_shared<k3::Texture>(image);
    }

    // Everything at once: textures, normal map, every light type, shadows, alpha test, double-sided blending.
    void drawScene(k3::IRasterizer& rasterizer)
    {
        k3::Camera camera;
        camera.position = {0.f, 1.5f, 4.f};
        camera.lookAt({0.f, 0.f, 0.f});

        k3::Environment environment;
        environment.ambient = {0.1f, 0.1f, 0.12f, 1.f};
        environment.lights = {
            k3::Light::directional({-0.4f, -1.f, -0.5f}, {1.f, 0.95f, 0.9f, 1.f}, 1.4f),
            k3::Light::point({1.5f, 0.8f, 1.f}, {1.f, 0.5f, 0.2f, 1.f}, 5.f, 6.f),
            k3::Light::spot({-2.f, 2.f, 1.f}, {1.f, -1.f, -0.5f}, {0.3f, 0.5f, 1.f, 1.f}, 8.f, 8.f, 0.3f, 0.45f),
        };
        environment.lights[0].castShadows = true;

        auto floor = std::make_shared<k3::Material>();
        floor->diffuseMap = checker(90, 200);
        auto shiny = std::make_shared<k3::Material>();
        shiny->diffuseMap = checker(40, 230);
        shiny->normalMap = k3::Texture::normalMapFromHeight(checker(0, 255)->image(), 2.f);
        shiny->specular = {0.6f, 0.6f, 0.6f, 1.f};
        shiny->shininess = 48.f;
        auto bumpySphere = sphere(32, 64);
        bumpySphere->computeTangents();
        auto glass = std::make_shared<k3::Material>();
        glass->diffuse = {0.3f, 0.6f, 1.f, 0.35f};
        glass->specular = {0.8f, 0.8f, 0.8f, 1.f};
        glass->alphaMode = k3::AlphaMode::Blend;
        glass->doubleSided = true;

        rasterizer.beginFrame(camera, environment);
        rasterizer.draw(quad(4.f, -1.f), floor, k3::Math::Matrix4::identity());
        rasterizer.draw(bumpySphere, shiny, k3::Math::Matrix4::translate({-0.6f, -0.2f, 0.f}) * k3::Math::Matrix4::scale(0.8f));
        rasterizer.draw(sphere(16, 32), glass, k3::Math::Matrix4::translate({0.9f, 0.f, 0.6f}) * k3::Math::Matrix4::scale(0.6f));
        rasterizer.endFrame();
    }

}

K3_TEST(gl_matches_the_software_backend)
{
    if (!openGLAvailable())
        return;

    constexpr std::uint32_t W = 200, H = 150;
    auto gpu = hardware(W, H);
    K3_REQUIRE(gpu);
    k3::SoftwareRasterizer cpu(W, H);

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
    std::cout << "    mean difference " << sum / pixels << ", > 8: " << 100.0 * over8 / pixels << "%, > 32: " << 100.0 * over32 / pixels << "%" << std::endl;

    K3_CHECK(sum / pixels < 1.0);
    K3_CHECK(over8 / pixels < 0.01);
    K3_CHECK(over32 / pixels < 0.002);
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

K3_TEST(gl_texture_changes_are_uploaded)
{
    if (!openGLAvailable())
        return;

    auto gpu = hardware(4, 4);
    K3_REQUIRE(gpu);

    k3::Camera camera;
    camera.position = {0.f, 0.f, 1.f};
    camera.projection = k3::Projection::Orthographic;
    auto texture = std::make_shared<k3::Texture>(k3::Image(1, 1, k3::Math::Color::Red));
    auto material = std::make_shared<k3::Material>();
    material->unlit = true;
    material->diffuseMap = texture;
    auto mesh = std::make_shared<k3::Mesh>(k3::Mesh{
        .positions = {{-1.f, -1.f, 0.f}, {1.f, -1.f, 0.f}, {1.f, 1.f, 0.f}, {-1.f, 1.f, 0.f}},
        .uvs = {{0.f, 1.f}, {1.f, 1.f}, {1.f, 0.f}, {0.f, 0.f}},
        .indices = {0, 1, 2, 0, 2, 3},
    });

    auto render = [&] {
        gpu->beginFrame(camera, k3::Environment{});
        gpu->draw(mesh, material, k3::Math::Matrix4::identity());
        gpu->endFrame();
        return gpu->readPixels();
    };

    K3_CHECK(render().texel(2, 2)[0] == 255);
    texture->setImage(k3::Image(1, 1, k3::Math::Color::Green));
    const k3::Image image = render();
    K3_CHECK(image.texel(2, 2)[0] == 0 && image.texel(2, 2)[1] == 255);
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

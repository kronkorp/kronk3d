#include "Test.hpp"
#include "render/CubeMap.hpp"
#include <kronk3d/render/software/SoftwareRasterizer.hpp>
#include <cmath>

namespace
{

    constexpr std::uint32_t SIZE = 16;
    constexpr float HALF_PI = 1.57079633f;

    std::shared_ptr<k3::Texture> solidFace(const k3::Math::Color& color, std::uint32_t size = 4)
    {
        auto texture = std::make_shared<k3::Texture>(k3::Image(size, size, color), k3::ColorSpace::Linear);

        texture->sampler.filter = k3::TextureFilter::Nearest;
        return texture;
    }

    // +X red, -X cyan, +Y green, -Y magenta, +Z blue, -Z yellow.
    k3::Skybox coloredSky()
    {
        k3::Skybox sky;
        const k3::Math::Color colors[6] = {k3::Math::Color::Red, k3::Math::Color::Cyan, k3::Math::Color::Green,
                                           k3::Math::Color::Magenta, k3::Math::Color::Blue, k3::Math::Color::Yellow};

        for (std::size_t i = 0; i < 6; ++i)
            sky.faces[i] = solidFace(colors[i]);
        return sky;
    }

    std::vector<k3::Math::Color> render(const k3::Camera& camera, const k3::Skybox& sky, const std::function<void(k3::IRasterizer&)>& scene = {})
    {
        k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
        k3::Environment environment;

        environment.clearColor = k3::Math::Color::Black;
        environment.skybox = sky;
        rasterizer.beginFrame(camera, environment);
        if (scene)
            scene(rasterizer);
        rasterizer.endFrame();
        return rasterizer.colorBuffer();
    }

    k3::Camera looking(float yaw, float pitch)
    {
        k3::Camera camera;

        camera.yaw = yaw;
        camera.pitch = pitch;
        return camera;
    }

    const k3::Math::Color& center(const std::vector<k3::Math::Color>& frame)
    {
        return frame[SIZE / 2 * SIZE + SIZE / 2];
    }

    bool is(const k3::Math::Color& a, const k3::Math::Color& b)
    {
        return std::abs(a.r - b.r) < 1e-4f && std::abs(a.g - b.g) < 1e-4f && std::abs(a.b - b.b) < 1e-4f;
    }

}

K3_TEST(cube_addressing_round_trips)
{
    for (int face = 0; face < 6; ++face) {
        for (float s : {0.1f, 0.5f, 0.9f}) {
            for (float t : {0.1f, 0.5f, 0.9f}) {
                const auto hit = k3::cube::lookup(k3::cube::direction(face, s, t));

                K3_CHECK(hit.face == face);
                K3_CHECK_NEAR(hit.s, s, 1e-5f);
                K3_CHECK_NEAR(hit.t, t, 1e-5f);
            }
        }
    }
}

K3_TEST(skybox_needs_six_square_faces_of_the_same_size)
{
    k3::Skybox sky;

    K3_CHECK(!sky.valid());
    sky = coloredSky();
    K3_CHECK(sky.valid());
    sky.faces[3] = solidFace(k3::Math::Color::Red, 8);
    K3_CHECK(!sky.valid());                                 // One face is bigger
    sky = coloredSky();
    sky.faces[2] = std::make_shared<k3::Texture>(k3::Image(4, 2));
    K3_CHECK(!sky.valid());                                 // Not square
    sky = coloredSky();
    sky.faces[5].reset();
    K3_CHECK(!sky.valid());
}

K3_TEST(skybox_faces_are_seen_in_the_right_directions)
{
    const auto sky = coloredSky();

    K3_CHECK(is(center(render(looking(0.f, 0.f), sky)), k3::Math::Color::Yellow));              // Looks toward -Z
    K3_CHECK(is(center(render(looking(HALF_PI, 0.f), sky)), k3::Math::Color::Red));             // Turned right: +X
    K3_CHECK(is(center(render(looking(-HALF_PI, 0.f), sky)), k3::Math::Color::Cyan));           // -X
    K3_CHECK(is(center(render(looking(2.f * HALF_PI, 0.f), sky)), k3::Math::Color::Blue));      // Behind: +Z
    K3_CHECK(is(center(render(looking(0.f, HALF_PI * 0.99f), sky)), k3::Math::Color::Green));   // Up: +Y
    K3_CHECK(is(center(render(looking(0.f, -HALF_PI * 0.99f), sky)), k3::Math::Color::Magenta));// Down: -Y
}

K3_TEST(skybox_images_are_upright_and_follow_opengl_addressing)
{
    // The -Z face, straight ahead: top half red, bottom half blue, left half green, right half white.
    auto sky = coloredSky();
    k3::Image image(4, 4);

    for (std::uint32_t y = 0; y < 4; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            std::uint8_t* texel = image.texel(x, y);

            texel[0] = y < 2 ? 255 : 0;
            texel[1] = x < 2 ? 255 : 0;
            texel[2] = y < 2 ? 0 : 255;
            texel[3] = 255;
        }
    }
    sky.faces[5] = std::make_shared<k3::Texture>(image, k3::ColorSpace::Linear);
    sky.faces[5]->sampler.filter = k3::TextureFilter::Nearest;

    const auto frame = render(looking(0.f, 0.f), sky);

    K3_CHECK(frame[2 * SIZE + 8].r == 1.f && frame[2 * SIZE + 8].b == 0.f);         // Top of the screen: top of the image
    K3_CHECK(frame[13 * SIZE + 8].r == 0.f && frame[13 * SIZE + 8].b == 1.f);       // Bottom
    // As in OpenGL, the right of the screen reads the left half of this face.
    K3_CHECK(frame[8 * SIZE + 13].g == 1.f);
    K3_CHECK(frame[8 * SIZE + 2].g == 0.f);
}

K3_TEST(skybox_gradient_goes_from_ground_to_horizon_to_zenith)
{
    const k3::Math::Color zenith{0.1f, 0.3f, 0.9f, 1.f}, horizon{0.8f, 0.9f, 1.f, 1.f}, ground{0.2f, 0.15f, 0.1f, 1.f};
    const auto sky = k3::Skybox::gradient(zenith, horizon, ground, 128);

    K3_REQUIRE(sky.valid());

    // The middle pixel is a couple of degrees below the exact horizon, hence the margin.
    auto near = [](const k3::Math::Color& a, const k3::Math::Color& b) {
        return std::abs(a.r - b.r) < 0.05f && std::abs(a.g - b.g) < 0.05f && std::abs(a.b - b.b) < 0.05f;
    };
    K3_CHECK(near(center(render(looking(0.f, 0.f), sky)), horizon));
    K3_CHECK(near(center(render(looking(1.f, 0.f), sky)), horizon));                       // Same all around
    K3_CHECK(near(center(render(looking(0.f, HALF_PI * 0.999f), sky)), zenith));
    K3_CHECK(near(center(render(looking(0.f, -HALF_PI * 0.999f), sky)), ground));

    const auto frame = render(looking(0.f, 0.f), sky);
    K3_CHECK(frame[1 * SIZE + 8].r < frame[8 * SIZE + 8].r);                               // Bluer (darker red) toward the top
}

K3_TEST(skybox_only_fills_what_the_geometry_leaves_uncovered)
{
    // A red quad over the left half of the view, 5 units ahead.
    auto quad = std::make_shared<k3::Mesh>(k3::Mesh{
        .positions = {{-10.f, -10.f, -5.f}, {0.f, -10.f, -5.f}, {0.f, 10.f, -5.f}, {-10.f, 10.f, -5.f}},
        .indices = {0, 1, 2, 0, 2, 3},
    });
    auto red = std::make_shared<k3::Material>();

    red->diffuse = k3::Math::Color::Red;
    red->unlit = true;

    const auto frame = render(looking(0.f, 0.f), coloredSky(), [&](k3::IRasterizer& r) { r.draw(quad, red, k3::Math::Matrix4::identity()); });

    K3_CHECK(is(frame[8 * SIZE + 2], k3::Math::Color::Red));            // Geometry stays in front
    K3_CHECK(is(frame[8 * SIZE + 13], k3::Math::Color::Yellow));        // Sky behind, instead of the clear color
}

K3_TEST(skybox_is_tinted_and_ignores_the_camera_position)
{
    auto sky = coloredSky();
    k3::Camera camera = looking(0.f, 0.f);

    sky.tint = {0.5f, 0.5f, 0.5f, 1.f};
    camera.position = {100.f, -50.f, 7.f};

    const auto c = center(render(camera, sky));

    K3_CHECK_NEAR(c.r, 0.5f, 1e-4f);        // Yellow dimmed
    K3_CHECK_NEAR(c.g, 0.5f, 1e-4f);
    K3_CHECK_NEAR(c.b, 0.f, 1e-4f);
}

K3_TEST(skybox_works_with_orthographic_cameras)
{
    k3::Camera camera = looking(HALF_PI, 0.f);

    camera.projection = k3::Projection::Orthographic;
    K3_CHECK(is(center(render(camera, coloredSky())), k3::Math::Color::Red));
}

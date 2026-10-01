#include "Test.hpp"
#include "render/software/SoftwareRasterizer.hpp"
#include "scene/Mesh.hpp"

namespace
{

    constexpr std::uint32_t SIZE = 32;

    // Orthographic camera seeing exactly the [-1, 1] square of the z = 0 plane.
    k3::Camera orthoCamera()
    {
        k3::Camera camera;
        camera.position = {0.f, 0.f, 1.f};
        camera.projection = k3::Projection::Orthographic;
        camera.orthoHeight = 2.f;
        camera.nearPlane = 0.1f;
        camera.farPlane = 10.f;
        return camera;
    }

    k3::Environment blackEnvironment()
    {
        k3::Environment environment;
        environment.clearColor = k3::Math::Color::Black;
        return environment;
    }

    // Counter-clockwise quad covering the whole view at height z.
    std::shared_ptr<k3::Mesh> quad(float z)
    {
        return std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{-1.f, -1.f, z}, {1.f, -1.f, z}, {1.f, 1.f, z}, {-1.f, 1.f, z}},
            .indices = {0, 1, 2, 0, 2, 3},
        });
    }

    std::shared_ptr<k3::Material> solid(const k3::Math::Color& color, k3::AlphaMode mode = k3::AlphaMode::Opaque)
    {
        auto material = std::make_shared<k3::Material>();
        material->diffuse = color;
        material->alphaMode = mode;
        material->unlit = true;
        return material;
    }

    const k3::Math::Color& center(const k3::SoftwareRasterizer& rasterizer)
    {
        return rasterizer.colorBuffer()[SIZE / 2 * SIZE + SIZE / 2];
    }

}

K3_TEST(software_fills_the_view)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(quad(0.f), solid(k3::Math::Color::Red), k3::Math::Matrix4::identity());
    rasterizer.endFrame();

    for (const auto& pixel : rasterizer.colorBuffer())
        K3_REQUIRE(pixel.r == 1.f && pixel.g == 0.f);
    K3_CHECK(rasterizer.stats().drawCalls == 1 && rasterizer.stats().triangles == 2);
}

K3_TEST(software_depth_test_keeps_nearest_regardless_of_order)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
    const auto identity = k3::Math::Matrix4::identity();

    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(quad(-0.5f), solid(k3::Math::Color::Red), identity);
    rasterizer.draw(quad(0.5f), solid(k3::Math::Color::Green), identity);
    rasterizer.endFrame();
    K3_CHECK(center(rasterizer).g == 1.f);

    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(quad(0.5f), solid(k3::Math::Color::Green), identity);
    rasterizer.draw(quad(-0.5f), solid(k3::Math::Color::Red), identity);
    rasterizer.endFrame();
    K3_CHECK(center(rasterizer).g == 1.f);
}

K3_TEST(software_back_faces_are_culled_unless_double_sided)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
    auto back = quad(0.f);
    auto material = solid(k3::Math::Color::Red);

    std::swap(back->indices[1], back->indices[2]);
    std::swap(back->indices[4], back->indices[5]);

    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(back, material, k3::Math::Matrix4::identity());
    rasterizer.endFrame();
    K3_CHECK(center(rasterizer).r == 0.f);

    material->doubleSided = true;
    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(back, material, k3::Math::Matrix4::identity());
    rasterizer.endFrame();
    K3_CHECK(center(rasterizer).r == 1.f);
}

K3_TEST(software_alpha_mask_discards_fragments)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(quad(0.f), solid({1.f, 0.f, 0.f, 0.2f}, k3::AlphaMode::Mask), k3::Math::Matrix4::identity());
    rasterizer.endFrame();
    K3_CHECK(center(rasterizer).r == 0.f);
}

K3_TEST(software_texture_repeat_wraps_uvs)
{
    k3::Image image(2, 1, k3::Math::Color::Red);
    image.texel(1, 0)[0] = 0;   // Right texel is black
    auto material = solid(k3::Math::Color::White);
    material->diffuseMap = std::make_shared<k3::Texture>(image);
    material->diffuseMap->sampler.filter = k3::TextureFilter::Nearest;

    // u in [1, 1.5] repeats to [0, 0.5]: the whole quad samples the red (left) texel.
    auto mesh = quad(0.f);
    mesh->uvs = {{1.f, 0.f}, {1.49f, 0.f}, {1.49f, 1.f}, {1.f, 1.f}};

    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(mesh, material, k3::Math::Matrix4::identity());
    rasterizer.endFrame();
    K3_CHECK(center(rasterizer).r == 1.f);
}

K3_TEST(software_shared_edge_is_blended_exactly_once)
{
    // The quad's diagonal goes exactly through pixel centers: without a fill rule they get blended twice.
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);

    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(quad(0.f), solid({1.f, 0.f, 0.f, 0.5f}, k3::AlphaMode::Blend), k3::Math::Matrix4::identity());
    rasterizer.endFrame();

    for (const auto& pixel : rasterizer.colorBuffer())
        K3_REQUIRE(std::abs(pixel.r - 0.5f) < 1e-6f);
}

K3_TEST(software_blended_draws_are_sorted_back_to_front)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
    const auto identity = k3::Math::Matrix4::identity();

    // Submitted near first: correct compositing is red over (green over black).
    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(quad(0.5f), solid({1.f, 0.f, 0.f, 0.5f}, k3::AlphaMode::Blend), identity);
    rasterizer.draw(quad(-0.5f), solid({0.f, 1.f, 0.f, 0.5f}, k3::AlphaMode::Blend), identity);
    rasterizer.endFrame();

    K3_CHECK_NEAR(center(rasterizer).r, 0.5f, 1e-6f);
    K3_CHECK_NEAR(center(rasterizer).g, 0.25f, 1e-6f);
}

K3_TEST(software_blended_geometry_is_hidden_behind_opaque)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
    const auto identity = k3::Math::Matrix4::identity();

    rasterizer.beginFrame(orthoCamera(), blackEnvironment());
    rasterizer.draw(quad(-0.5f), solid({1.f, 0.f, 0.f, 0.5f}, k3::AlphaMode::Blend), identity);
    rasterizer.draw(quad(0.5f), solid(k3::Math::Color::Blue), identity);
    rasterizer.endFrame();

    K3_CHECK(center(rasterizer).r == 0.f && center(rasterizer).b == 1.f);
}

K3_TEST(software_clips_geometry_crossing_the_near_plane)
{
    // A floor going from under the camera to far ahead: crosses the near plane.
    k3::Camera camera;
    camera.position = {0.f, 1.f, 0.f};
    auto floor = std::make_shared<k3::Mesh>(k3::Mesh{
        .positions = {{-50.f, 0.f, 50.f}, {50.f, 0.f, 50.f}, {50.f, 0.f, -50.f}, {-50.f, 0.f, -50.f}},
        .indices = {0, 1, 2, 0, 2, 3},
    });

    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
    rasterizer.beginFrame(camera, blackEnvironment());
    rasterizer.draw(floor, solid(k3::Math::Color::Red), k3::Math::Matrix4::identity());
    rasterizer.endFrame();

    // Bottom row sees the floor, top row the sky.
    K3_CHECK(rasterizer.colorBuffer()[(SIZE - 1) * SIZE + SIZE / 2].r == 1.f);
    K3_CHECK(rasterizer.colorBuffer()[SIZE / 2].r == 0.f);
}

K3_TEST(software_read_pixels_is_srgb_encoded)
{
    k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
    k3::Environment environment;
    environment.clearColor = {0.5f, 0.f, 1.f, 1.f};

    rasterizer.beginFrame(orthoCamera(), environment);
    rasterizer.endFrame();

    const k3::Image image = rasterizer.readPixels();
    K3_REQUIRE(image.width == SIZE && image.height == SIZE);
    K3_CHECK(image.pixels[0] == 188);   // linearToSrgb(0.5) * 255
    K3_CHECK(image.pixels[1] == 0);
    K3_CHECK(image.pixels[2] == 255);
    K3_CHECK(image.pixels[3] == 255);
}

K3_TEST(factory_creates_the_software_backend)
{
    auto rasterizer = k3::createRasterizer(k3::Backend::Software, {.width = 16, .height = 8});

    K3_REQUIRE(rasterizer);
    K3_CHECK((*rasterizer)->backend() == k3::Backend::Software);
    K3_CHECK((*rasterizer)->width() == 16 && (*rasterizer)->height() == 8);
}

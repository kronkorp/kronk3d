#include "Test.hpp"
#include "Rasterizer.hpp"
#include "scene/Mesh.hpp"

namespace
{

    constexpr std::size_t SIZE = 32;

    // Quad covering the whole NDC square at depth z, counter-clockwise.
    k3::Mesh quad(float z)
    {
        return k3::Mesh{
            .positions = {{-1.f, -1.f, z}, {1.f, -1.f, z}, {1.f, 1.f, z}, {-1.f, 1.f, z}},
            .indices = {0, 1, 2, 0, 2, 3},
        };
    }

    k3::Material solid(const k3::Math::Color& color)
    {
        k3::Material material;
        material.diffuse = color;
        return material;
    }

    const k3::Viewport VIEWPORT{0, SIZE, 0, SIZE};

    const k3::Math::Color& center(const k3::Rasterizer& rasterizer)
    {
        return rasterizer.framebuffer()[SIZE / 2 * SIZE + SIZE / 2];
    }

}

K3_TEST(rasterizer_fills_the_viewport)
{
    k3::Rasterizer rasterizer(SIZE, SIZE);

    rasterizer.clear(k3::Math::Color::Black);
    rasterizer.draw(quad(0.f), solid(k3::Math::Color::Red), VIEWPORT);

    for (const auto& pixel : rasterizer.framebuffer())
        K3_REQUIRE(pixel.r == 1.f && pixel.g == 0.f);
}

K3_TEST(rasterizer_depth_test_keeps_nearest_regardless_of_order)
{
    k3::Rasterizer rasterizer(SIZE, SIZE);

    rasterizer.clear(k3::Math::Color::Black);
    rasterizer.draw(quad(0.5f), solid(k3::Math::Color::Red), VIEWPORT);
    rasterizer.draw(quad(-0.5f), solid(k3::Math::Color::Green), VIEWPORT);
    K3_CHECK(center(rasterizer).g == 1.f);

    rasterizer.clear(k3::Math::Color::Black);
    rasterizer.draw(quad(-0.5f), solid(k3::Math::Color::Green), VIEWPORT);
    rasterizer.draw(quad(0.5f), solid(k3::Math::Color::Red), VIEWPORT);
    K3_CHECK(center(rasterizer).g == 1.f);
}

K3_TEST(rasterizer_back_face_culling)
{
    k3::Rasterizer rasterizer(SIZE, SIZE);
    k3::Mesh back = quad(0.f);

    std::swap(back.indices[1], back.indices[2]);
    std::swap(back.indices[4], back.indices[5]);

    rasterizer.clear(k3::Math::Color::Black);
    rasterizer.draw(back, solid(k3::Math::Color::Red), VIEWPORT);
    K3_CHECK(center(rasterizer).r == 0.f);

    rasterizer.draw(back, solid(k3::Math::Color::Red), VIEWPORT, k3::Math::Matrix4::identity(), k3::Rasterizer::Cull::None);
    K3_CHECK(center(rasterizer).r == 1.f);
}

K3_TEST(rasterizer_alpha_mask_discards_fragments)
{
    k3::Rasterizer rasterizer(SIZE, SIZE);
    k3::Material material = solid({1.f, 0.f, 0.f, 0.2f});

    material.alphaMode = k3::AlphaMode::Mask;
    rasterizer.clear(k3::Math::Color::Black);
    rasterizer.draw(quad(0.f), material, VIEWPORT);
    K3_CHECK(center(rasterizer).r == 0.f);
}

K3_TEST(rasterizer_texture_repeat_wraps_uvs)
{
    k3::Image image(2, 1, k3::Math::Color::Red);
    image.texel(1, 0)[0] = 0;   // Right texel is black
    k3::Material material;
    material.diffuseMap = std::make_shared<k3::Texture>(image);
    material.diffuseMap->sampler.filter = k3::TextureFilter::Nearest;

    // u in [1, 1.5] repeats to [0, 0.5]: the whole quad samples the red (left) texel.
    k3::Mesh mesh = quad(0.f);
    mesh.uvs = {{1.f, 0.f}, {1.49f, 0.f}, {1.49f, 1.f}, {1.f, 1.f}};

    k3::Rasterizer rasterizer(SIZE, SIZE);
    rasterizer.clear(k3::Math::Color::Black);
    rasterizer.draw(mesh, material, VIEWPORT);
    K3_CHECK(center(rasterizer).r == 1.f);
}

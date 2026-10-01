#include "Test.hpp"
#include "render/software/Sampling.hpp"
#include "render/software/SoftwareRasterizer.hpp"
#include "scene/Texture.hpp"

namespace
{

    const k3::Math::Vector2f NO_DERIVATIVE{0.f, 0.f};

    k3::Image grey(std::uint32_t w, std::uint32_t h, const std::uint8_t* values)
    {
        k3::Image image(w, h);

        for (std::uint32_t i = 0; i < w * h; ++i) {
            image.pixels[i * 4 + 0] = image.pixels[i * 4 + 1] = image.pixels[i * 4 + 2] = values[i];
            image.pixels[i * 4 + 3] = 255;
        }
        return image;
    }

    k3::Image checkerboard(std::uint32_t size)
    {
        k3::Image image(size, size);

        for (std::uint32_t y = 0; y < size; ++y)
            for (std::uint32_t x = 0; x < size; ++x)
                for (int c = 0; c < 3; ++c)
                    image.texel(x, y)[c] = ((x + y) % 2) ? 255 : 0;
        return image;
    }

    // Renders a full-view quad textured with `texture` (unlit) on a 16x16 target, returns the center pixel.
    k3::Math::Color renderTextured(std::shared_ptr<k3::Texture> texture)
    {
        constexpr std::uint32_t SIZE = 16;
        k3::Camera camera;
        camera.position = {0.f, 0.f, 1.f};
        camera.projection = k3::Projection::Orthographic;
        camera.orthoHeight = 2.f;

        auto mesh = std::make_shared<k3::Mesh>(k3::Mesh{
            .positions = {{-1.f, -1.f, 0.f}, {1.f, -1.f, 0.f}, {1.f, 1.f, 0.f}, {-1.f, 1.f, 0.f}},
            .uvs = {{0.f, 1.f}, {1.f, 1.f}, {1.f, 0.f}, {0.f, 0.f}},
            .indices = {0, 1, 2, 0, 2, 3},
        });
        auto material = std::make_shared<k3::Material>();
        material->unlit = true;
        material->diffuseMap = std::move(texture);

        k3::SoftwareRasterizer rasterizer(SIZE, SIZE);
        rasterizer.beginFrame(camera, k3::Environment{});
        rasterizer.draw(mesh, material, k3::Math::Matrix4::identity());
        rasterizer.endFrame();
        return rasterizer.colorBuffer()[SIZE / 2 * SIZE + SIZE / 2];
    }

}

K3_TEST(texture_mip_chain_goes_down_to_1x1)
{
    const std::uint8_t values[15] = {};
    k3::Texture texture(grey(5, 3, values));

    K3_REQUIRE(texture.levelCount() == 3);
    K3_CHECK(texture.level(1).width == 2 && texture.level(1).height == 1);
    K3_CHECK(texture.level(2).width == 1 && texture.level(2).height == 1);
    // Past the end: the smallest level.
    K3_CHECK(texture.level(3).width == 1);
    K3_CHECK(k3::Texture().levelCount() == 1 && k3::Texture().image().empty());
}

K3_TEST(texture_mipmaps_average_srgb_as_light)
{
    const std::uint8_t values[4] = {0, 255, 255, 0};

    // Half black, half white is 50% light: 188 once sRGB-encoded, not 128.
    K3_CHECK(k3::Texture(grey(2, 2, values), k3::ColorSpace::Srgb).level(1).pixels[0] == 188);
    K3_CHECK(k3::Texture(grey(2, 2, values), k3::ColorSpace::Linear).level(1).pixels[0] == 128);
}

K3_TEST(sampler_bilinear_blends_neighbour_texels)
{
    const std::uint8_t values[2] = {0, 255};
    k3::Texture texture(grey(2, 1, values), k3::ColorSpace::Linear);
    texture.sampler.wrapU = texture.sampler.wrapV = k3::TextureWrap::ClampToEdge;

    // u = 0.5 is halfway between the two texel centers (0.25 and 0.75).
    K3_CHECK_NEAR(k3::sw::sampleTexture(&texture, {0.5f, 0.5f}, NO_DERIVATIVE, NO_DERIVATIVE).r, 0.5f, 1e-6f);
    K3_CHECK_NEAR(k3::sw::sampleTexture(&texture, {0.375f, 0.5f}, NO_DERIVATIVE, NO_DERIVATIVE).r, 0.25f, 1e-6f);
    // Clamped: before the first texel center, only the first texel.
    K3_CHECK(k3::sw::sampleTexture(&texture, {0.1f, 0.5f}, NO_DERIVATIVE, NO_DERIVATIVE).r == 0.f);

    // Repeat: between the last and the first texel.
    texture.sampler.wrapU = k3::TextureWrap::Repeat;
    K3_CHECK_NEAR(k3::sw::sampleTexture(&texture, {0.f, 0.5f}, NO_DERIVATIVE, NO_DERIVATIVE).r, 0.5f, 1e-6f);
}

K3_TEST(sampler_wrap_modes_with_nearest)
{
    const std::uint8_t values[4] = {0, 85, 170, 255};
    k3::Texture texture(grey(4, 1, values), k3::ColorSpace::Linear);
    texture.sampler.filter = k3::TextureFilter::Nearest;

    auto at = [&](float u, k3::TextureWrap wrap) {
        texture.sampler.wrapU = wrap;
        return k3::sw::sampleTexture(&texture, {u, 0.5f}, NO_DERIVATIVE, NO_DERIVATIVE).r * 255.f;
    };

    K3_CHECK_NEAR(at(1.125f, k3::TextureWrap::Repeat), 0.f, 1e-3f);
    K3_CHECK_NEAR(at(-0.125f, k3::TextureWrap::Repeat), 255.f, 1e-3f);
    K3_CHECK_NEAR(at(1.125f, k3::TextureWrap::MirroredRepeat), 255.f, 1e-3f);
    K3_CHECK_NEAR(at(1.375f, k3::TextureWrap::MirroredRepeat), 170.f, 1e-3f);
    K3_CHECK_NEAR(at(5.f, k3::TextureWrap::ClampToEdge), 255.f, 1e-3f);
    K3_CHECK_NEAR(at(-3.f, k3::TextureWrap::ClampToEdge), 0.f, 1e-3f);
}

K3_TEST(sampler_uses_coarser_mips_when_minified)
{
    // 256 texels across 16 pixels: a 1-texel checkerboard must average out to grey, not alias.
    auto texture = std::make_shared<k3::Texture>(checkerboard(256), k3::ColorSpace::Linear);
    K3_CHECK_NEAR(renderTextured(texture).r, 0.5f, 0.01f);

    texture->sampler.mipmaps = k3::MipmapFilter::None;
    texture->sampler.filter = k3::TextureFilter::Nearest;
    const float aliased = renderTextured(texture).r;
    K3_CHECK(aliased == 0.f || aliased == 1.f);
}

K3_TEST(sampler_lod_follows_the_derivatives)
{
    // 4x4 checker: level 0 holds the pattern, level 2 (1x1) its average.
    const std::uint8_t values[16] = {255, 0, 255, 0, 0, 255, 0, 255, 255, 0, 255, 0, 0, 255, 0, 255};
    k3::Texture texture(grey(4, 4, values), k3::ColorSpace::Linear);
    texture.sampler.filter = k3::TextureFilter::Nearest;
    texture.sampler.mipmaps = k3::MipmapFilter::Nearest;

    // One texel per pixel: level 0 (the white texel at (0, 0)).
    K3_CHECK(k3::sw::sampleTexture(&texture, {0.1f, 0.1f}, {0.25f, 0.f}, {0.f, 0.25f}).r == 1.f);
    // Four texels per pixel: level 2 (1x1, the average of the checker).
    K3_CHECK_NEAR(k3::sw::sampleTexture(&texture, {0.1f, 0.1f}, {1.f, 0.f}, {0.f, 1.f}).r, 128.f / 255.f, 1e-6f);
}

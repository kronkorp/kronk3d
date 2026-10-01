#include "Sampling.hpp"
#include "utils/Srgb.hpp"
#include <algorithm>
#include <cmath>

namespace
{

    using k3::Math::Color;

    // Brings a coordinate back to a small range first, so the texel indices below cannot overflow.
    float reduce(float t, k3::TextureWrap mode) noexcept
    {
        if (!std::isfinite(t))
            return 0.f;
        switch (mode) {
            case k3::TextureWrap::Repeat:
                return t - std::floor(t);
            case k3::TextureWrap::MirroredRepeat:
                return t - 2.f * std::floor(t * 0.5f);
            case k3::TextureWrap::ClampToEdge:
                break;
        }
        return std::clamp(t, -1.f, 2.f);
    }

    // `i` comes from a coordinate already reduced to [0, 1) (repeat), [0, 2) (mirrored) or [-1, 2]
    // (clamp), plus at most one texel of bilinear footprint: a couple of compares instead of a modulo.
    int wrapIndex(int i, int size, k3::TextureWrap mode) noexcept
    {
        switch (mode) {
            case k3::TextureWrap::Repeat:
                return i < 0 ? i + size : (i >= size ? i - size : i);
            case k3::TextureWrap::MirroredRepeat: {
                const int period = 2 * size;
                const int m = i < 0 ? i + period : (i >= period ? i - period : i);
                return m < size ? m : period - 1 - m;
            }
            case k3::TextureWrap::ClampToEdge:
                break;
        }
        return std::clamp(i, 0, size - 1);
    }

    // `srgb`: the sRGB decode table, or null for linear textures.
    Color fetch(const k3::Image& image, int x, int y, const float* srgb) noexcept
    {
        const std::uint8_t* t = image.texel(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));

        if (srgb)
            return {srgb[t[0]], srgb[t[1]], srgb[t[2]], t[3] * (1.f / 255.f)};
        return {t[0] * (1.f / 255.f), t[1] * (1.f / 255.f), t[2] * (1.f / 255.f), t[3] * (1.f / 255.f)};
    }

    Color sampleLevel(const k3::Image& image, const k3::Sampler& sampler, float u, float v, const float* srgb) noexcept
    {
        const int w = static_cast<int>(image.width);
        const int h = static_cast<int>(image.height);
        const float su = u * w;
        const float sv = v * h;

        if (sampler.filter == k3::TextureFilter::Nearest) {
            return fetch(image,
                wrapIndex(static_cast<int>(std::floor(su)), w, sampler.wrapU),
                wrapIndex(static_cast<int>(std::floor(sv)), h, sampler.wrapV),
                srgb);
        }

        // Bilinear: blend the 4 texels whose centers surround the sample point.
        const float fu = su - 0.5f, fv = sv - 0.5f;
        const float x0f = std::floor(fu), y0f = std::floor(fv);
        const float ax = fu - x0f, ay = fv - y0f;
        const int x0 = static_cast<int>(x0f), y0 = static_cast<int>(y0f);
        const int xa = wrapIndex(x0, w, sampler.wrapU), xb = wrapIndex(x0 + 1, w, sampler.wrapU);
        const int ya = wrapIndex(y0, h, sampler.wrapV), yb = wrapIndex(y0 + 1, h, sampler.wrapV);

        const Color top = (1.f - ax) * fetch(image, xa, ya, srgb) + ax * fetch(image, xb, ya, srgb);
        const Color bottom = (1.f - ax) * fetch(image, xa, yb, srgb) + ax * fetch(image, xb, yb, srgb);
        return (1.f - ay) * top + ay * bottom;
    }

}

k3::Math::Color k3::sw::sampleTexture(const Texture* texture, const Math::Vector2f& uv, const Math::Vector2f& duvdx, const Math::Vector2f& duvdy) noexcept
{
    if (!texture || texture->image().empty())
        return Color::White;

    const Sampler& sampler = texture->sampler;
    const float* srgb = texture->colorSpace() == ColorSpace::Srgb ? srgb::decodeTable() : nullptr;
    const float u = reduce(uv.x, sampler.wrapU);
    const float v = reduce(uv.y, sampler.wrapV);

    // OpenGL level of detail: log2 of the longest screen-pixel footprint, in texels.
    const float w = static_cast<float>(texture->width());
    const float h = static_cast<float>(texture->height());
    const float lengthX = (duvdx.x * w) * (duvdx.x * w) + (duvdx.y * h) * (duvdx.y * h);
    const float lengthY = (duvdy.x * w) * (duvdy.x * w) + (duvdy.y * h) * (duvdy.y * h);
    const float lod = 0.5f * std::log2(std::max(lengthX, lengthY));
    const std::size_t levels = texture->levelCount();

    // Magnified (or no mipmaps): full-resolution level.
    if (!(lod > 0.f) || sampler.mipmaps == MipmapFilter::None || levels == 1)
        return sampleLevel(texture->image(), sampler, u, v, srgb);

    const float maxLod = static_cast<float>(levels - 1);
    const float clamped = std::min(lod, maxLod);

    if (sampler.mipmaps == MipmapFilter::Nearest)
        return sampleLevel(texture->level(static_cast<std::size_t>(clamped + 0.5f)), sampler, u, v, srgb);

    const std::size_t base = static_cast<std::size_t>(clamped);
    const float blend = clamped - static_cast<float>(base);
    const Color c0 = sampleLevel(texture->level(base), sampler, u, v, srgb);

    if (blend <= 0.f || base + 1 >= levels)
        return c0;
    return (1.f - blend) * c0 + blend * sampleLevel(texture->level(base + 1), sampler, u, v, srgb);
}

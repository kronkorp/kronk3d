#include "Sampling.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

    // Linear values are quantized to 16 bits before encoding: fine enough that no sRGB level is skipped.
    constexpr std::size_t ENCODE_SIZE = 65536;

    struct SrgbTables
    {
        float decode[256];
        std::vector<std::uint8_t> encode;

        SrgbTables() : encode(ENCODE_SIZE)
        {
            for (int i = 0; i < 256; ++i)
                decode[i] = k3::Math::srgbToLinear(i / 255.f);
            for (std::size_t i = 0; i < ENCODE_SIZE; ++i)
                encode[i] = static_cast<std::uint8_t>(std::lround(k3::Math::linearToSrgb(i / float(ENCODE_SIZE - 1)) * 255.f));
        }
    };

    const SrgbTables& tables()
    {
        static const SrgbTables instance;
        return instance;
    }

    float wrap(float t, k3::TextureWrap mode) noexcept
    {
        switch (mode) {
            case k3::TextureWrap::Repeat:
                return t - std::floor(t);
            case k3::TextureWrap::MirroredRepeat: {
                const float m = t - 2.f * std::floor(t * 0.5f);
                return m > 1.f ? 2.f - m : m;
            }
            case k3::TextureWrap::ClampToEdge:
                break;
        }
        return std::clamp(t, 0.f, 1.f);
    }

}

float k3::sw::srgbByteToLinear(std::uint8_t value) noexcept
{
    return tables().decode[value];
}

std::uint8_t k3::sw::linearToSrgbByte(float value) noexcept
{
    // Also maps NaN to 0, since every comparison with NaN is false.
    const float clamped = value > 0.f ? (value < 1.f ? value : 1.f) : 0.f;
    return tables().encode[static_cast<std::size_t>(clamped * (ENCODE_SIZE - 1) + 0.5f)];
}

k3::Math::Color k3::sw::sampleTexture(const Texture* texture, const Math::Vector2f& uv, float /*lod*/) noexcept
{
    if (!texture || texture->image().empty())
        return Math::Color::White;

    const Image& image = texture->image();
    const float u = wrap(uv.x, texture->sampler.wrapU);
    const float v = wrap(uv.y, texture->sampler.wrapV);
    const auto x = std::min(static_cast<std::uint32_t>(u * image.width), image.width - 1);
    const auto y = std::min(static_cast<std::uint32_t>(v * image.height), image.height - 1);
    const std::uint8_t* texel = image.texel(x, y);

    if (texture->colorSpace() == ColorSpace::Srgb) {
        const auto& decode = tables().decode;
        return {decode[texel[0]], decode[texel[1]], decode[texel[2]], texel[3] / 255.f};
    }
    return Math::Color::fromRGB(texel[0], texel[1], texel[2], texel[3]);
}

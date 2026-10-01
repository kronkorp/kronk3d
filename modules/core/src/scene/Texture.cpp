#include "Texture.hpp"
#include "Vector.hpp"
#include "utils/Log.hpp"
#include "utils/Srgb.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace
{

    // 2x2 box filter. Odd sizes repeat their last row/column. sRGB channels are averaged as light
    // (decoded to linear first), otherwise a black/white checker would shrink to a too-dark grey.
    k3::Image downsample(const k3::Image& src, k3::ColorSpace colorSpace)
    {
        k3::Image dst;
        dst.width = std::max(1u, src.width / 2);
        dst.height = std::max(1u, src.height / 2);
        dst.pixels.resize(static_cast<std::size_t>(dst.width) * dst.height * 4);

        const bool srgb = colorSpace == k3::ColorSpace::Srgb;
        const float* decode = k3::srgb::decodeTable();
        const std::uint8_t* encode = k3::srgb::encodeTable();

        for (std::uint32_t y = 0; y < dst.height; ++y) {
            const std::uint32_t y0 = std::min(2 * y, src.height - 1), y1 = std::min(2 * y + 1, src.height - 1);

            for (std::uint32_t x = 0; x < dst.width; ++x) {
                const std::uint32_t x0 = std::min(2 * x, src.width - 1), x1 = std::min(2 * x + 1, src.width - 1);
                const std::uint8_t* texels[4] = {src.texel(x0, y0), src.texel(x1, y0), src.texel(x0, y1), src.texel(x1, y1)};
                std::uint8_t* out = dst.texel(x, y);

                for (int c = 0; c < 4; ++c) {
                    if (srgb && c < 3) {
                        float sum = 0.f;
                        for (const auto* t : texels)
                            sum += decode[t[c]];
                        out[c] = k3::srgb::encode(sum * 0.25f, encode);
                    } else {
                        const unsigned sum = texels[0][c] + texels[1][c] + texels[2][c] + texels[3][c];
                        out[c] = static_cast<std::uint8_t>((sum + 2) / 4);
                    }
                }
            }
        }
        return dst;
    }

}

k3::Texture::Texture()
    : m_levels(1)
{
}

k3::Texture::Texture(Image image, ColorSpace colorSpace)
    : m_colorSpace(colorSpace)
{
    setImage(std::move(image));
}

std::shared_ptr<k3::Texture> k3::Texture::load(const std::filesystem::path& path, ColorSpace colorSpace)
{
    auto image = Image::load(path);

    if (!image) {
        log(LogLevel::Error, "Failed to load texture {}", image.error());
        return nullptr;
    }
    return std::make_shared<Texture>(std::move(*image), colorSpace);
}

void k3::Texture::setImage(Image image)
{
    m_hasTransparency = image.hasTransparency();
    m_levels.clear();
    m_levels.push_back(std::move(image));

    while (!m_levels.back().empty() && (m_levels.back().width > 1 || m_levels.back().height > 1))
        m_levels.push_back(downsample(m_levels.back(), m_colorSpace));
    ++m_version;
}

std::shared_ptr<k3::Texture> k3::Texture::normalMapFromHeight(const Image& height, float strength)
{
    Image normals(height.width, height.height);
    const auto w = static_cast<std::int64_t>(height.width), h = static_cast<std::int64_t>(height.height);
    auto at = [&](std::int64_t x, std::int64_t y) {
        return height.texel(static_cast<std::uint32_t>((x + w) % w), static_cast<std::uint32_t>((y + h) % h))[0] / 255.f;
    };

    for (std::int64_t y = 0; y < h; ++y) {
        for (std::int64_t x = 0; x < w; ++x) {
            // Central differences; image y grows downward, normal map +y points up.
            const float dx = (at(x + 1, y) - at(x - 1, y)) * 0.5f * strength;
            const float dy = (at(x, y + 1) - at(x, y - 1)) * 0.5f * strength;
            const auto n = Math::Vector3f::normalize({-dx, dy, 1.f});
            std::uint8_t* texel = normals.texel(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));

            texel[0] = static_cast<std::uint8_t>(std::lround((n.x * 0.5f + 0.5f) * 255.f));
            texel[1] = static_cast<std::uint8_t>(std::lround((n.y * 0.5f + 0.5f) * 255.f));
            texel[2] = static_cast<std::uint8_t>(std::lround((n.z * 0.5f + 0.5f) * 255.f));
            texel[3] = 255;
        }
    }
    return std::make_shared<Texture>(std::move(normals), ColorSpace::Linear);
}

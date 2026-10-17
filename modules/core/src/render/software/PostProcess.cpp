#include "PostProcess.hpp"
#include <algorithm>
#include <cmath>

namespace
{

    using k3::Math::Color;

    constexpr float FXAA_SPAN_MAX   = 8.f;
    constexpr float FXAA_REDUCE_MUL = 1.f / 8.f;
    constexpr float FXAA_REDUCE_MIN = 1.f / 128.f;

    // Perceptual luma: edge detection works on how bright things look, not on linear light.
    float luma(const Color& c)
    {
        return std::sqrt(0.299f * c.r + 0.587f * c.g + 0.114f * c.b);
    }

    struct Source
    {
        const std::vector<Color>& pixels;
        int width, height;

        const Color& at(int x, int y) const
        {
            return pixels[static_cast<std::size_t>(std::clamp(y, 0, height - 1)) * width + std::clamp(x, 0, width - 1)];
        }

        // Bilinear lookup at a position in pixels (centers at i + 0.5), clamped to the edges.
        Color sample(float x, float y) const
        {
            const float fx = x - 0.5f, fy = y - 0.5f;
            const float x0 = std::floor(fx), y0 = std::floor(fy);
            const float ax = fx - x0, ay = fy - y0;
            const int ix = static_cast<int>(x0), iy = static_cast<int>(y0);

            const Color top = (1.f - ax) * at(ix, iy) + ax * at(ix + 1, iy);
            const Color bottom = (1.f - ax) * at(ix, iy + 1) + ax * at(ix + 1, iy + 1);
            return (1.f - ay) * top + ay * bottom;
        }
    };

}

void k3::sw::downsample2x2(const std::vector<Math::Color>& source, std::uint32_t width, std::uint32_t height, std::vector<Math::Color>& destination, ThreadPool& pool)
{
    const std::size_t sourceWidth = static_cast<std::size_t>(width) * 2;

    destination.resize(static_cast<std::size_t>(width) * height);
    pool.parallelFor(height, [&](std::size_t y) {
        const Color* top = source.data() + 2 * y * sourceWidth;
        const Color* bottom = top + sourceWidth;

        for (std::size_t x = 0; x < width; ++x)
            destination[y * width + x] = 0.25f * (top[2 * x] + top[2 * x + 1] + bottom[2 * x] + bottom[2 * x + 1]);
    });
}

void k3::sw::fxaa(const std::vector<Math::Color>& source, std::uint32_t width, std::uint32_t height, std::vector<Math::Color>& destination, ThreadPool& pool)
{
    const Source image{source, static_cast<int>(width), static_cast<int>(height)};
    const int w = image.width, h = image.height;

    // Every pixel's luma is read by its 4 diagonal neighbours: compute it once.
    std::vector<float> lumas(source.size());
    pool.parallelFor(height, [&](std::size_t row) {
        for (std::size_t i = row * width; i < (row + 1) * width; ++i)
            lumas[i] = luma(source[i]);
    });
    auto lumaAt = [&](int x, int y) {
        return lumas[static_cast<std::size_t>(std::clamp(y, 0, h - 1)) * w + std::clamp(x, 0, w - 1)];
    };

    destination.resize(static_cast<std::size_t>(width) * height);
    pool.parallelFor(height, [&](std::size_t row) {
        const int y = static_cast<int>(row);

        for (int x = 0; x < w; ++x) {
            const float nw = lumaAt(x - 1, y - 1);
            const float ne = lumaAt(x + 1, y - 1);
            const float sw = lumaAt(x - 1, y + 1);
            const float se = lumaAt(x + 1, y + 1);
            const Color& center = image.at(x, y);
            const float m = lumaAt(x, y);
            const float lumaMin = std::min({m, nw, ne, sw, se});
            const float lumaMax = std::max({m, nw, ne, sw, se});

            // Direction along the edge: perpendicular to the luma gradient.
            float dirX = -((nw + ne) - (sw + se));
            float dirY = (nw + sw) - (ne + se);
            const float reduce = std::max((nw + ne + sw + se) * (0.25f * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
            const float scale = 1.f / (std::min(std::abs(dirX), std::abs(dirY)) + reduce);
            dirX = std::clamp(dirX * scale, -FXAA_SPAN_MAX, FXAA_SPAN_MAX);
            dirY = std::clamp(dirY * scale, -FXAA_SPAN_MAX, FXAA_SPAN_MAX);

            const float cx = static_cast<float>(x) + 0.5f, cy = static_cast<float>(y) + 0.5f;
            const Color a = 0.5f * (image.sample(cx + dirX * (1.f / 3.f - 0.5f), cy + dirY * (1.f / 3.f - 0.5f))
                                  + image.sample(cx + dirX * (2.f / 3.f - 0.5f), cy + dirY * (2.f / 3.f - 0.5f)));
            const Color b = 0.5f * a + 0.25f * (image.sample(cx - dirX * 0.5f, cy - dirY * 0.5f)
                                              + image.sample(cx + dirX * 0.5f, cy + dirY * 0.5f));
            const float lumaB = luma(b);

            Color result = (lumaB < lumaMin || lumaB > lumaMax) ? a : b;
            result.a = center.a;
            destination[static_cast<std::size_t>(y) * width + x] = result;
        }
    });
}

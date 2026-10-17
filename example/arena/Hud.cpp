#include "Hud.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace
{

    constexpr int GLYPH_WIDTH = 3, GLYPH_HEIGHT = 5, GLYPH_STRIDE = 4;     // One blank column between glyphs
    constexpr float PIXEL = 4.f;                                            // Screen pixels per font texel

    // Rows of each digit, top to bottom, three bits each.
    constexpr int FONT[10][GLYPH_HEIGHT] = {
        {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
        {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
    };

    constexpr float MAP_SIZE = 160.f;
    constexpr float MARGIN = 16.f;

}

Hud::Hud()
{
    k3::Image image(GLYPH_STRIDE * 10, GLYPH_HEIGHT, k3::Math::Color::Transparent);

    for (int digit = 0; digit < 10; ++digit) {
        for (int row = 0; row < GLYPH_HEIGHT; ++row) {
            for (int column = 0; column < GLYPH_WIDTH; ++column) {
                if (FONT[digit][row] & (4 >> column)) {
                    std::uint8_t* texel = image.texel(static_cast<std::uint32_t>(digit * GLYPH_STRIDE + column), static_cast<std::uint32_t>(row));

                    texel[0] = texel[1] = texel[2] = texel[3] = 255;
                }
            }
        }
    }
    m_digits = std::make_shared<k3::Texture>(std::move(image));
    m_digits->sampler.filter = k3::TextureFilter::Nearest;      // Crisp pixels
    m_digits->sampler.mipmaps = k3::MipmapFilter::None;
}

void Hud::number(k3::IRasterizer& rasterizer, int value, float x, float y, const k3::Math::Color& tint) const
{
    const std::string text = std::to_string(std::max(value, 0));

    // A dark backing, then the digits.
    const float width = static_cast<float>(text.size()) * GLYPH_STRIDE * PIXEL;
    rasterizer.drawRect({x - PIXEL, y - PIXEL, width + PIXEL, GLYPH_HEIGHT * PIXEL + 2.f * PIXEL}, {0.f, 0.f, 0.f, 0.45f});
    for (const char c : text) {
        const float source = static_cast<float>((c - '0') * GLYPH_STRIDE);

        rasterizer.drawImage(m_digits, {x, y, GLYPH_WIDTH * PIXEL, GLYPH_HEIGHT * PIXEL}, {source, 0.f, GLYPH_WIDTH, GLYPH_HEIGHT}, tint);
        x += GLYPH_STRIDE * PIXEL;
    }
}

void Hud::draw(k3::IRasterizer& rasterizer, const Level& level, const k3::Math::Vector3f& player, float cameraYaw, int fps, const k3::FrameStats& stats) const
{
    // Top left: frames per second (white), draw calls (grey) and draws skipped by the frustum culling (yellow).
    number(rasterizer, fps, MARGIN, MARGIN, k3::Math::Color::White);
    number(rasterizer, static_cast<int>(stats.drawCalls), MARGIN, MARGIN + 36.f, {0.6f, 0.65f, 0.7f, 1.f});
    number(rasterizer, static_cast<int>(stats.culledDraws), MARGIN, MARGIN + 72.f, {1.f, 0.85f, 0.2f, 1.f});

    // Top right: the minimap.
    const float left = static_cast<float>(rasterizer.width()) - MAP_SIZE - MARGIN, top = MARGIN;
    const float scale = MAP_SIZE / (2.f * Level::HALF_SIZE);
    auto toMap = [&](float x, float z) { return std::pair{left + (x + Level::HALF_SIZE) * scale, top + (z + Level::HALF_SIZE) * scale}; };

    rasterizer.drawRect({left - 2.f, top - 2.f, MAP_SIZE + 4.f, MAP_SIZE + 4.f}, {1.f, 1.f, 1.f, 0.5f});
    rasterizer.drawRect({left, top, MAP_SIZE, MAP_SIZE}, {0.02f, 0.03f, 0.05f, 0.7f});
    for (const Box& box : level.solids()) {
        if (box.min.x < -Level::HALF_SIZE || box.max.x > Level::HALF_SIZE || box.min.z < -Level::HALF_SIZE || box.max.z > Level::HALF_SIZE)
            continue;       // The outer walls are the frame
        const auto [x, z] = toMap(box.min.x, box.min.z);
        const float tall = box.max.y;
        const k3::Math::Color color = box.glass ? k3::Math::Color{0.3f, 0.7f, 1.f, 0.8f} : (tall > 2.5f ? k3::Math::Color{0.75f, 0.75f, 0.8f, 0.95f} : k3::Math::Color{0.55f, 0.4f, 0.28f, 0.95f});

        rasterizer.drawRect({x, z, std::max((box.max.x - box.min.x) * scale, 1.f), std::max((box.max.z - box.min.z) * scale, 1.f)}, color);
    }

    // The character (yellow) and where the camera looks (cyan, ahead of it).
    const auto [px, pz] = toMap(player.x, player.z);
    const float ahead = 9.f;

    rasterizer.drawRect({px + std::sin(cameraYaw) * ahead - 2.f, pz - std::cos(cameraYaw) * ahead - 2.f, 4.f, 4.f}, {0.2f, 1.f, 1.f, 1.f});
    rasterizer.drawRect({px - 3.f, pz - 3.f, 6.f, 6.f}, {1.f, 0.9f, 0.1f, 1.f});
}

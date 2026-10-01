/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** CPU-side RGBA8 image
*/
#pragma once

#include "Color.hpp"
#include "utils/Result.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace k3
{

    // 8-bit RGBA pixels, row-major, top row first.
    struct Image
    {
        std::uint32_t width{}, height{};
        std::vector<std::uint8_t> pixels{};

        Image() = default;
        Image(std::uint32_t w, std::uint32_t h, const Math::Color& fill = Math::Color::Black);

        [[nodiscard]] bool empty() const noexcept
        {
            return width == 0 || height == 0;
        }

        [[nodiscard]] std::uint8_t* texel(std::uint32_t x, std::uint32_t y) noexcept
        {
            return pixels.data() + (static_cast<std::size_t>(y) * width + x) * 4;
        }

        [[nodiscard]] const std::uint8_t* texel(std::uint32_t x, std::uint32_t y) const noexcept
        {
            return pixels.data() + (static_cast<std::size_t>(y) * width + x) * 4;
        }

        // True if at least one texel is not fully opaque.
        [[nodiscard]] bool hasTransparency() const noexcept;

        // PNG, JPEG, BMP, TGA, PSD, GIF, HDR, PIC, PNM (anything stb_image reads). Always converted to RGBA8.
        static k3::Result<Image> load(const std::filesystem::path& path);
        static k3::Result<Image> loadFromMemory(const void* data, std::size_t size);

        bool savePNG(const std::filesystem::path& path) const;
    };

}

/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Texture: image data + how to sample it
*/
#pragma once

#include "Image.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>

namespace k3
{

    enum class TextureFilter {
        Nearest,
        Linear
    };

    enum class TextureWrap {
        Repeat,
        MirroredRepeat,
        ClampToEdge
    };

    // Color textures (albedo) are authored in sRGB, data textures (opacity, masks) are linear.
    enum class ColorSpace {
        Srgb,
        Linear
    };

    struct Sampler
    {
        TextureFilter filter = TextureFilter::Linear;
        TextureWrap   wrapU  = TextureWrap::Repeat;
        TextureWrap   wrapV  = TextureWrap::Repeat;
        bool          mipmaps = true;
    };

    // UV convention: (0, 0) is the top-left texel of the image, v grows downward.
    class Texture
    {
        public:
            Texture() = default;
            explicit Texture(Image image, ColorSpace colorSpace = ColorSpace::Srgb);

            // Returns nullptr (and logs) when the file cannot be read.
            static std::shared_ptr<Texture> load(const std::filesystem::path& path, ColorSpace colorSpace = ColorSpace::Srgb);

            [[nodiscard]] const Image& image() const noexcept { return m_image; }
            [[nodiscard]] std::uint32_t width() const noexcept { return m_image.width; }
            [[nodiscard]] std::uint32_t height() const noexcept { return m_image.height; }
            [[nodiscard]] ColorSpace colorSpace() const noexcept { return m_colorSpace; }
            [[nodiscard]] bool hasTransparency() const noexcept { return m_hasTransparency; }

            // Bumped on every modification, so GPU backends know when to re-upload.
            [[nodiscard]] std::uint64_t version() const noexcept { return m_version; }

            void setImage(Image image);

            Sampler sampler{};

        private:
            Image         m_image{};
            ColorSpace    m_colorSpace = ColorSpace::Srgb;
            bool          m_hasTransparency = false;
            std::uint64_t m_version = 0;
    };

}

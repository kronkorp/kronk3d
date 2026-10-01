/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Texture: image data + mip chain + how to sample it
*/
#pragma once

#include "Image.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace k3
{

    // Filtering inside one mip level.
    enum class TextureFilter {
        Nearest,    // Closest texel
        Linear      // Bilinear blend of the 4 closest texels
    };

    // Filtering between mip levels, when the texture is minified.
    enum class MipmapFilter {
        None,       // Always read the full-resolution level (aliases when minified)
        Nearest,    // Closest level
        Linear      // Blend of the two closest levels (with TextureFilter::Linear: trilinear)
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

    // Same semantics as OpenGL samplers, so every backend filters identically.
    struct Sampler
    {
        TextureFilter filter  = TextureFilter::Linear;
        MipmapFilter  mipmaps = MipmapFilter::Linear;
        TextureWrap   wrapU   = TextureWrap::Repeat;
        TextureWrap   wrapV   = TextureWrap::Repeat;
    };

    // UV convention: (0, 0) is the top-left corner of the image, v grows downward.
    // The full mip chain (down to 1x1) is built whenever the image is set, averaging in linear space.
    class Texture
    {
        public:
            Texture();
            explicit Texture(Image image, ColorSpace colorSpace = ColorSpace::Srgb);

            // Returns nullptr (and logs) when the file cannot be read.
            static std::shared_ptr<Texture> load(const std::filesystem::path& path, ColorSpace colorSpace = ColorSpace::Srgb);

            // Tangent-space normal map (OpenGL convention, linear) from a height map: the red channel is
            // the height in [0, 1], `strength` scales the slopes (a full 0 -> 1 rise over one texel tilts
            // the normal by atan(strength)). Wraps at the borders, as tiling textures do.
            static std::shared_ptr<Texture> normalMapFromHeight(const Image& height, float strength = 4.f);

            [[nodiscard]] const Image& image() const noexcept { return level(0); }
            [[nodiscard]] std::uint32_t width() const noexcept { return image().width; }
            [[nodiscard]] std::uint32_t height() const noexcept { return image().height; }
            [[nodiscard]] ColorSpace colorSpace() const noexcept { return m_colorSpace; }
            [[nodiscard]] bool hasTransparency() const noexcept { return m_hasTransparency; }

            // Level 0 is the image itself, each next level is half the size of the previous one.
            // Indices past the last level return the last (smallest) one.
            [[nodiscard]] std::size_t levelCount() const noexcept { return m_levels.size(); }
            [[nodiscard]] const Image& level(std::size_t index) const noexcept
            {
                return m_levels[index < m_levels.size() ? index : m_levels.size() - 1];
            }

            // Bumped on every modification, so GPU backends know when to re-upload.
            [[nodiscard]] std::uint64_t version() const noexcept { return m_version; }

            void setImage(Image image);

            Sampler sampler{};

        private:
            std::vector<Image> m_levels{};                      // Never empty
            ColorSpace         m_colorSpace = ColorSpace::Srgb;
            bool               m_hasTransparency = false;
            std::uint64_t      m_version = 0;
    };

}

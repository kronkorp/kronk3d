/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** CPU texture sampling and sRGB conversions
*/
#pragma once

#include "Color.hpp"
#include "Vector.hpp"
#include "scene/Texture.hpp"
#include <cstdint>

namespace k3::sw
{

    // Table-driven sRGB transfer functions (exact pow() is far too slow per pixel).
    float        srgbByteToLinear(std::uint8_t value) noexcept;
    std::uint8_t linearToSrgbByte(float value) noexcept;

    // Linear-space color of `texture` at `uv` (sRGB textures are decoded). A null texture samples as
    // white so it does not tint what it multiplies. `lod` is the mip level to read.
    Math::Color sampleTexture(const Texture* texture, const Math::Vector2f& uv, float lod) noexcept;

}

/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** CPU texture sampling (OpenGL sampler semantics)
*/
#pragma once

#include "Color.hpp"
#include "Vector.hpp"
#include "scene/Texture.hpp"

namespace k3::sw
{

    // Linear-space color of `texture` at `uv` (sRGB textures are decoded). `duvdx` / `duvdy` are the
    // screen-space derivatives of uv, which select the mip level like the OpenGL specification does.
    // A null texture samples as white so it does not tint what it multiplies.
    Math::Color sampleTexture(const Texture* texture, const Math::Vector2f& uv, const Math::Vector2f& duvdx, const Math::Vector2f& duvdy) noexcept;

}

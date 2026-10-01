/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Per-frame scene settings shared by every draw
*/
#pragma once

#include "Color.hpp"
#include "scene/Light.hpp"
#include <cstddef>
#include <vector>

namespace k3
{

    // Lights past this count are ignored (fixed-size uniform arrays on the GPU backends).
    inline constexpr std::size_t MAX_LIGHTS = 8;

    // All colors are linear (see Math::Color::fromSRGB).
    struct Environment
    {
        Math::Color        clearColor = {0.02f, 0.02f, 0.025f, 1.f};
        Math::Color        ambient    = {0.1f, 0.1f, 0.1f, 1.f};      // Multiplied by the surface albedo
        std::vector<Light> lights{};
    };

}

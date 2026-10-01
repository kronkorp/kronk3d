/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Per-frame scene settings shared by every draw
*/
#pragma once

#include "Bounds.hpp"
#include "Color.hpp"
#include "scene/Light.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace k3
{

    // Lights past this count are ignored (fixed-size uniform arrays on the GPU backends).
    inline constexpr std::size_t MAX_LIGHTS = 8;

    // Shadows come from the first directional light with castShadows set, through cascaded shadow
    // maps: the camera's view range is split into `cascades` slices, each with its own orthographic
    // shadow map looking along the light, so nearby shadows get most of the resolution.
    struct ShadowSettings
    {
        std::uint32_t  resolution  = 1024;      // Each cascade's map is resolution x resolution
        int            cascades    = 3;         // 1 to 4. 1: a single map covering the whole scene
        float          maxDistance = 0.f;       // View distance shadows reach; 0: up to the farthest drawn object
        float          splitLambda = 0.75f;     // Cascade splits: 0 = uniform, 1 = logarithmic (more detail up close)
        float          depthBias   = 0.002f;    // Subtracted from the receiver depth, in shadow-map depth units [0, 1]
        float          normalBias  = 1.5f;      // Receivers are pushed along their normal by this many shadow texels
        int            pcfRadius   = 1;         // Percentage-closer filtering over (2r + 1)^2 texels
        Math::Bounds3f bounds{};                // If set: a single fixed map covering this world region (no cascades)
    };

    // All colors are linear (see Math::Color::fromSRGB).
    struct Environment
    {
        Math::Color        clearColor = {0.02f, 0.02f, 0.025f, 1.f};
        Math::Color        ambient    = {0.1f, 0.1f, 0.1f, 1.f};      // Multiplied by the surface albedo
        std::vector<Light> lights{};
        ShadowSettings     shadows{};
    };

}

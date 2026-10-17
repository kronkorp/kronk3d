/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Per-frame scene settings shared by every draw
*/
#pragma once

#include <kronk3d/math/Bounds.hpp>
#include <kronk3d/math/Color.hpp>
#include <kronk3d/render/Skybox.hpp>
#include <kronk3d/scene/Light.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace k3
{

    // How the scene's colors reach the screen. None (the default) clamps them to [0, 1]. Reinhard and ACES
    // render the scene in high dynamic range instead (a bright sun, emissive materials or a `Skybox::tint`
    // above 1 keep their detail) and map it to [0, 1] once the whole scene is drawn, before anti-aliasing:
    // Reinhard is x / (1 + x), ACES a filmic S-curve. The 2D overlay is not mapped.
    enum class ToneMapping {
        None,
        Reinhard,
        ACES
    };

    // Lights past this count are ignored (fixed-size uniform arrays on the GPU backends).
    inline constexpr std::size_t MAX_LIGHTS = 8;

    // Exponential height fog, integrated along each view ray: the density thins out with altitude, so
    // the fog pools in low ground and the view through it is right wherever the camera is, inside the
    // layer or above it. A surface at distance d (straight, with no height difference) keeps
    // e^(-density * d) of its color; the rest is `color`. Everything drawn is fogged, transparent
    // surfaces and unlit materials included; the background (clear color or sky) is not, so give `color`
    // the horizon's color for distant geometry to melt into it. It is not light scattering: no light shafts.
    struct Fog
    {
        Math::Color color         = {0.7f, 0.75f, 0.8f, 1.f};   // Linear
        float       density       = 0.f;    // Per world unit, at baseHeight; 0 turns the fog off
        float       heightFalloff = 0.f;    // Density is divided by e per 1 / heightFalloff units of altitude; 0: uniform fog
        float       baseHeight    = 0.f;    // Altitude where the density is `density`
    };

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
        Fog                fog{};
        Skybox             skybox{};                                  // When valid(), drawn instead of clearColor behind the geometry
        Math::Color        ambient    = {0.1f, 0.1f, 0.1f, 1.f};      // Multiplied by the surface albedo
        // Hemisphere light: when set, the ambient light goes from `ambient` for surfaces facing up to this
        // color for surfaces facing down (a sky above, a floor below), with their normal's height in between.
        std::optional<Math::Color> ambientGround{};
        ToneMapping        toneMapping = ToneMapping::None;
        float              exposure = 1.f;        // Multiplies the scene's colors before the curve (not used by None)
        std::vector<Light> lights{};
        ShadowSettings     shadows{};
    };

}

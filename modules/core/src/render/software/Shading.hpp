/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Fragment shading of the software rasterizer
*/
#pragma once

#include "Color.hpp"
#include "Vector.hpp"
#include "render/Environment.hpp"
#include "scene/Camera.hpp"
#include "scene/Material.hpp"
#include <cstddef>

namespace k3::sw
{

    // Interpolated surface attributes at one pixel, in world space.
    struct Fragment
    {
        Math::Vector3f position;
        Math::Vector3f normal;          // Not normalized
        Math::Vector2f uv;
        Math::Vector2f duvdx, duvdy;    // Screen-space derivatives of uv (mip level selection)
        Math::Color    color;           // Vertex color
        bool           frontFacing;
    };

    // A light with everything precomputed that does not depend on the fragment.
    struct PreparedLight
    {
        LightType      type;
        Math::Vector3f position;
        Math::Vector3f toLight;         // Directional: normalized direction toward the light
        Math::Vector3f direction;       // Spot: normalized direction the light travels
        Math::Color    radiance;        // color * intensity
        float          range;
        float          cosInner, cosOuter;
    };

    // Per-frame data every fragment needs, prepared once in beginFrame().
    struct ShadingContext
    {
        Math::Vector3f cameraPosition{};
        Math::Vector3f viewDirection{};     // Orthographic cameras: the view vector is the same everywhere
        bool           orthographic = false;
        Math::Color    ambient{};
        PreparedLight  lights[MAX_LIGHTS]{};
        std::size_t    lightCount = 0;

        static ShadingContext prepare(const Camera& camera, const Environment& environment);
    };

    // Opacity of the fragment (diffuse alpha * vertex alpha * textures), used by the alpha test.
    float coverage(const Material& material, const Fragment& fragment) noexcept;

    // Blinn-Phong: ambient * albedo + emissive + sum over lights of
    //     radiance * attenuation * (albedo * N.L + specular * (N.H)^shininess)
    // Point/spot attenuation: (1 - (d / range)^4)^2 / (d^2 + 1), spots fade smoothly between the cones.
    // Returns the final linear color, alpha included.
    Math::Color shade(const ShadingContext& context, const Material& material, const Fragment& fragment) noexcept;

}

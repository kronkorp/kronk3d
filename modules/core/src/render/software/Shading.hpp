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

namespace k3::sw
{

    // Interpolated surface attributes at one pixel, in world space.
    struct Fragment
    {
        Math::Vector3f position;
        Math::Vector3f normal;          // Not normalized
        Math::Vector2f uv;
        Math::Color    color;           // Vertex color
        float          lod;             // Texture mip level
        bool           frontFacing;
    };

    // Per-frame data every fragment needs, prepared once in endFrame().
    struct ShadingContext
    {
        Math::Vector3f cameraPosition;
        Math::Color    ambient;

        static ShadingContext prepare(const Camera& camera, const Environment& environment);
    };

    // Opacity of the fragment (diffuse alpha * vertex alpha * textures), used by the alpha test.
    float coverage(const Material& material, const Fragment& fragment) noexcept;

    // Final linear color, alpha included.
    Math::Color shade(const ShadingContext& context, const Material& material, const Fragment& fragment) noexcept;

}

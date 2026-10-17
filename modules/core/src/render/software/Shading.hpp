/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Fragment shading of the software rasterizer
*/
#pragma once

#include <kronk3d/math/Color.hpp>
#include <kronk3d/math/Matrix.hpp>
#include <kronk3d/math/Vector.hpp>
#include <kronk3d/render/Environment.hpp>
#include "render/ShadowFit.hpp"
#include <kronk3d/scene/Camera.hpp>
#include <kronk3d/scene/Material.hpp>
#include <cstddef>
#include <cstdint>

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
        Math::Vector4f tangent;         // xyz + handedness; zero when the draw has no normal map
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

    // Depth seen from the shadow-casting light, one map per cascade, rendered before the frame is shaded.
    struct ShadowMap
    {
        struct Cascade
        {
            const float*  depth = nullptr;          // size x size, in [0, 1], row 0 on the light's +y side
            Math::Matrix4 viewProjection = Math::Matrix4::identity();   // World -> light clip space (orthographic)
            float         normalOffset = 0.f;       // In world units
            float         splitDistance = 0.f;      // Used up to this view distance
        };

        Cascade        cascades[MAX_SHADOW_CASCADES]{};
        std::size_t    count = 0;
        std::uint32_t  size = 0;
        float          depthBias = 0.f;             // In depth units
        int            pcfRadius = 0;
        Math::Vector3f cameraPosition{}, cameraForward{};   // To measure view distances
    };

    // Fog of the frame, with the density already brought to the camera's altitude.
    struct FogState
    {
        bool        enabled = false;
        Math::Color color{};
        float       density = 0.f;      // At the camera's altitude
        float       falloff = 0.f;      // Per unit of altitude
    };

    // Fraction of a surface's color that survives the fog between the camera and `position`, in [0, 1]:
    // exp(-integral of the density along the ray). The OpenGL fragment shader mirrors this.
    float fogVisibility(const FogState& fog, const Math::Vector3f& cameraPosition, const Math::Vector3f& position) noexcept;

    // Per-frame data every fragment needs, prepared once in beginFrame().
    struct ShadingContext
    {
        Math::Vector3f cameraPosition{};
        Math::Vector3f viewDirection{};     // Orthographic cameras: the view vector is the same everywhere
        bool           orthographic = false;
        Math::Color    ambient{};
        bool           hemisphere = false;      // Ambient light between `ambient` (up) and `ambientGround` (down)
        Math::Color    ambientGround{};
        FogState       fog{};
        PreparedLight  lights[MAX_LIGHTS]{};
        std::size_t    lightCount = 0;
        int            shadowLight = -1;    // Index in `lights` of the light the shadow map belongs to
        ShadowMap      shadow{};            // Filled in endFrame(), once the shadow pass is done

        static ShadingContext prepare(const Camera& camera, const Environment& environment);
    };

    // Fraction of the (2r + 1)^2 shadow-map texels around `position` that see the light, in [0, 1], in
    // the first cascade reaching its view distance. `normal` (normalized, facing the viewer) pushes the
    // lookup off the surface against shadow acne.
    float shadowVisibility(const ShadowMap& shadow, const Math::Vector3f& position, const Math::Vector3f& normal) noexcept;

    // Opacity of the fragment (diffuse alpha * vertex alpha * textures), used by the alpha test.
    float coverage(const Material& material, const Fragment& fragment) noexcept;

    // Blinn-Phong: ambient * albedo + emissive + sum over lights of
    //     radiance * attenuation * (albedo * N.L + specular * (N.H)^shininess)
    // Point/spot attenuation: (1 - (d / range)^4)^2 / (d^2 + 1), spots fade smoothly between the cones.
    // Returns the final linear color, alpha included.
    Math::Color shade(const ShadingContext& context, const Material& material, const Fragment& fragment) noexcept;

}

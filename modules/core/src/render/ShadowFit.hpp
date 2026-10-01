/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Light-space projections of directional shadow maps (shared by every backend)
*/
#pragma once

#include "Bounds.hpp"
#include "Matrix.hpp"
#include "Vector.hpp"
#include "render/Environment.hpp"
#include "scene/Camera.hpp"
#include <cstddef>
#include <vector>

namespace k3
{

    inline constexpr std::size_t MAX_SHADOW_CASCADES = 4;

    struct ShadowCascade
    {
        Math::Matrix4 viewProjection;   // World -> light clip space
        float         texelSize;        // World size of one shadow-map texel
        float         splitDistance;    // Covers view distances (along the camera's forward axis) up to this
    };

    // The shadow maps to render this frame, nearest first. Each cascade bounds one slice of the camera's
    // view range with a sphere (so its size does not change as the camera turns), snapped to the texel
    // grid (so shadows do not shimmer as it moves), and reaches toward the light to the end of
    // `sceneBounds` (so casters outside the slice still cast into it). `settings.bounds` or a single
    // cascade give one map over the given bounds or the whole scene. Empty when there is nothing to shadow.
    std::vector<ShadowCascade> fitShadowCascades(
        const ShadowSettings& settings, const Camera& camera, float aspectRatio,
        const Math::Bounds3f& sceneBounds, const Math::Vector3f& direction
    );

}
